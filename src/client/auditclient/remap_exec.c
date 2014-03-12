/*
This file is part of Spindle.  For copyright information see the COPYRIGHT 
file in the top level directory, or at 
https://github.com/hpc/Spindle/blob/master/COPYRIGHT

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License (as published by the Free Software
Foundation) version 2.1 dated February 1999.  This program is distributed in the
hope that it will be useful, but WITHOUT ANY WARRANTY; without even the IMPLIED
WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms 
and conditions of the GNU General Public License for more details.  You should 
have received a copy of the GNU Lesser General Public License along with this 
program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA
*/

#define _GNU_SOURCE
#include <elf.h>
#include <link.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/syscall.h>

#include "ldcs_api.h"
#include "client.h"
#include "client_heap.h"
#include "client_api.h"

static int pagesize;

static int fetchPhdrs(ElfW(Addr) *aout_base, ElfW(Phdr) **phdrs, unsigned int *phdrs_size)
{
   pid_t pid;
   char auxvpath[64];
   int fd = 0, result, i;
   ElfW(auxv_t) auxv;
   ElfW(Phdr) *phdr;
   ElfW(Addr) phdr_offset = 0;
   
   pid = getpid();
   debug_printf2("Reading auxv for process %d for exec remapping\n", pid);

   snprintf(auxvpath, 64, "/proc/%d/auxv", (int) pid);
   auxvpath[63] = '\0';
   fd = open(auxvpath, O_RDONLY);
   if (fd == -1) {
      err_printf("Could not open auxv at %s, skipping exec remapping: %s\n",
                 auxvpath, strerror(errno));
      return -1;
   }

   *phdrs = NULL;
   *phdrs_size = 0;
   for (;;) {
      do {
         result = read(fd, &auxv, sizeof(auxv));
      } while (result == -1 && errno == EINTR);
      if (result == -1) {
         err_printf("Error reading from %s: %s\n", auxvpath, strerror(errno));
         close(fd);
         return -1;
      }
      if (result == 0)
         break;
      if (auxv.a_type == AT_PHDR) {
         *phdrs = (ElfW(Phdr) *) auxv.a_un.a_val;
      }
      else if (auxv.a_type == AT_PHNUM) {
         *phdrs_size = (unsigned int) auxv.a_un.a_val;
      }      
   } while (result > 0);
   close(fd);

   if (!*phdrs) {
      err_printf("Could not find phdrs pointer in auxv\n");
      return -1;
   }
   if (!*phdrs_size) {
      err_printf("Could not find phdrs_size in auxv\n");
      return -1;
   }

   for (i = 0, phdr = *phdrs; i < *phdrs_size; i++, phdr++) {
      if (phdr->p_type == PT_PHDR) {
         phdr_offset = (ElfW(Addr)) phdr->p_vaddr;
         break;
      }
   }
   if (i == *phdrs_size) {
      err_printf("Failed to find PT_PHDR in program headers\n");
      return -1;
   }

   *aout_base = (ElfW(Addr)) (((unsigned long) *phdrs) - phdr_offset);

   debug_printf("Remapping a.out.  %u program headers at %p.  Program base at %lx\n",
                *phdrs_size, *phdrs, (unsigned long) *aout_base);

   return 0;
}

static int openRelocatedExec(int ldcsid)
{
   char orig_exec[MAX_PATH_LEN+1];
   char *reloc_exec;
   char proc_pid_exec[64];
   ssize_t result;
   int fd;

   snprintf(proc_pid_exec, 64, "/proc/%d/exe", getpid());
   proc_pid_exec[63] = '\0';
   memset(orig_exec, 0, sizeof(orig_exec));
   result = readlink(proc_pid_exec, orig_exec, MAX_PATH_LEN);
   if (result == -1) {
      err_printf("Could not read link %s for exec remapping: %s\n",
                 proc_pid_exec, strerror(errno));
      return -1;
   }
   
   debug_printf2("Exec remapping requesting relocation of file %s\n",
                 orig_exec);
   send_file_query(ldcsid, orig_exec, &reloc_exec);
   debug_printf2("Exec remapping returned %s -> %s\n", orig_exec, reloc_exec);

   fd = open(reloc_exec, O_RDWR);
   if (fd == -1) {
      err_printf("Error opening relocated executable %s: %s\n", reloc_exec, strerror(errno));
      spindle_free(reloc_exec);
      return -1;
   }

   spindle_free(reloc_exec);
   return fd;
}

static ElfW(Addr) page_align(ElfW(Addr) addr) {
   return addr & ~(pagesize - 1);
}

/**
 * This function re-maps the executable's LOAD segments to reference a
 * new file.  We use this primarily to create processes with a /proc/PID/exe
 * that points at the original executable, but pulls pages from our relocated
 * executable.  
 *
 * We should mostly just do this during debugging, as it's usually debuggers that
 * care about seeing a correct /proc/PID/exe.
 **/
void remap_executable(int ldcsid)
{
   ElfW(Addr) aout_base;
   ElfW(Phdr) *phdrs, *p;
   unsigned int phdrs_size, i;
   int result;
   int exe_fd;

   pagesize = getpagesize();

   //Bind mmap in PLT/GOT tables before using in exe remapping.
   mmap(NULL, 0, 0, 0, -1, 0);
   
   result = fetchPhdrs(&aout_base, &phdrs, &phdrs_size);
   if (result == -1) {
      err_printf("Skipping remapping of executable\n");
      return;
   }

   exe_fd = openRelocatedExec(ldcsid);
   if (exe_fd == -1) {
      return;
   }

   for (p = phdrs, i = 0; i < phdrs_size; p++, i++) {
      ElfW(Addr) vaddr, foffset, addr;
      unsigned long fsize;
      int flags = 0;
      void *mresult;

      if (p->p_type != PT_LOAD)
         continue;
      vaddr = p->p_vaddr;
      foffset = p->p_offset;
      fsize = p->p_filesz;
      flags = (p->p_flags & PF_X) ? PROT_EXEC : 0;
      flags |= (p->p_flags & PF_W) ? PROT_WRITE : 0;
      flags |= (p->p_flags & PF_R) ? PROT_READ : 0;
      
      addr = page_align(vaddr + aout_base);
      foffset = page_align(foffset);
      fsize += vaddr + aout_base - addr;

      if (!p->p_offset) {
         //The Linux kernel gets /proc/pid/exe from a FD that it opened during process startup.
         // If we close the last mapping to the original exec, then that FD will be auto-closed
         // and /proc/pid/exe will start erroring.  Thus if we won't remap the first page (the one
         // containing an elf header) of the executable.  The kernel's already touched this page
         // anyways, so we're not going to incur any additional overhead.
         if (fsize < pagesize)
            continue;
         addr += pagesize;
         fsize -= pagesize;
         foffset = pagesize;
      }
   
      mresult = mmap((void *) addr, fsize, (int) flags, MAP_PRIVATE | MAP_FIXED, exe_fd, foffset);
      if (mresult == MAP_FAILED) {
         err_printf("mmap(%lx, %lu, %d, %d %d, %lu) failed when re-mapping a.out: %s\n",
                    addr, (unsigned long) fsize, flags, MAP_PRIVATE | MAP_FIXED, exe_fd, (unsigned long) foffset,
                    strerror(errno));
         close(exe_fd);
         return;
      }
      else if (mresult != (void *) addr) {
         err_printf("mmap(%lx, %lu, %d, %d %d, %lu) = %p didn't map to the correct place\n",
                    addr, (unsigned long) fsize, flags, MAP_PRIVATE | MAP_FIXED, exe_fd, (unsigned long) foffset,
                    mresult);
         close(exe_fd);
         return;
      }
      if (p->p_memsz > p->p_filesz) {
         //Zero out the first page of bss after mapping
         ElfW(Addr) last_data_page = aout_base + p->p_vaddr + p->p_filesz;
         int amount_to_zero = last_data_page & (pagesize-1);
         if (amount_to_zero != 0)
            amount_to_zero = pagesize - amount_to_zero;
         debug_printf3("Zero'ing %d bytes at %lx in a.out remapping because memsz (%lu) != filesz (%lu)\n",
                       amount_to_zero, last_data_page, p->p_memsz, p->p_filesz);
         if (amount_to_zero)
            memset((void *) last_data_page, 0, amount_to_zero);
      }
      debug_printf("Remapped LOAD segment at addr %p at offset %lx\n", mresult, foffset);
   }

   close(exe_fd);
}
