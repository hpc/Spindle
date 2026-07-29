/*
This file is part of Spindle.  For copyright information see the COPYRIGHT
file in the top level directory, or at
https://github.com/hpc/Spindle/blob/master/COPYRIGHT

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License (as published by the Free Software
Foundation) version 2.1 dated February 1999.  This program is distributed in the
hope that it will be useful, but WITHOUT ANY WARRANTY; without even the IMPLIED
WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms
and conditions of the GNU Lesser General Public License for more details.  You should
have received a copy of the GNU Lesser General Public License along with this
program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA
*/

#define _GNU_SOURCE

#include "crash_lib_offset.h"
#include "crash_fmt.h"

#include <elf.h>
#include <fcntl.h>
#include <link.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/auxv.h>
#include <sys/syscall.h>
#include <unistd.h>

#define MAX_PATH_LEN 4096

struct r_debug_ext_mirror {
   struct r_debug base;
   void *r_next;
};

/* It turns out that we can't get at the program headers via
   dl_phdr_iterate in a signal handler, because dl_phdr_iterate
   calls malloc, which is not async-signal-safe, and furthermore
   dl_phdr_iterate only iterates the objects loaded in the current
   namespace, so when called from the Spindle auditclient we only
   see the auditclient and its own libc. Instead, we have to read
   the program headers from the file. */

static const ElfW(Phdr) *exe_auxv_phdrs;
static unsigned long exe_auxv_phnum;
static char exe_path_cache[MAX_PATH_LEN + 1];
static char *exe_path_cached;

/* Get the path to the current executable. This is used for
   the <library> part of <library>+<offset> when the address
   in in the executable. */
static char *get_executable_path(void)
{
   long r;

   if (exe_path_cached)
      return exe_path_cached;

   r = syscall(SYS_readlinkat, AT_FDCWD, "/proc/self/exe",
               exe_path_cache, (size_t) MAX_PATH_LEN);
   if (r < 0 || r > MAX_PATH_LEN)
      return exe_path_cached = (char *) "[EXECUTABLE]";
   exe_path_cache[r] = '\0';
   return exe_path_cached = exe_path_cache;
}

/* Check the program headers of the file at path for the given address */
static int pc_in_object_file(const char *path, unsigned long base,
                             unsigned long pc)
{
   ElfW(Ehdr) ehdr;
   ElfW(Phdr) phdr;
   long fd, n;
   unsigned int i;
   unsigned long off;
   int found = 0;

   fd = syscall(SYS_openat, AT_FDCWD, path, O_RDONLY | O_CLOEXEC, 0);
   if (fd < 0)
      return 0;

   n = syscall(SYS_pread64, fd, &ehdr, sizeof(ehdr), (off_t) 0);
   if (n != (long) sizeof(ehdr))
      goto done;
   if (memcmp(ehdr.e_ident, ELFMAG, SELFMAG) != 0)
      goto done;
   if (ehdr.e_phnum == 0)
      goto done;

   off = ehdr.e_phoff;
   for (i = 0; i < ehdr.e_phnum; i++, off += ehdr.e_phentsize) {
      n = syscall(SYS_pread64, fd, &phdr, sizeof(phdr), (off_t) off);
      if (n != (long) sizeof(phdr))
         break;
      if (phdr.p_type != PT_LOAD)
         continue;
      unsigned long start = base + phdr.p_vaddr;
      if (pc >= start && pc < start + phdr.p_memsz) {
         found = 1;
         break;
      }
   }

done:
   syscall(SYS_close, fd);
   return found;
}

static int pc_in_exe(unsigned long base, unsigned long pc)
{
   unsigned long i, start;

   if (!exe_auxv_phdrs)
      return 0;
   for (i = 0; i < exe_auxv_phnum; i++) {
      if (exe_auxv_phdrs[i].p_type != PT_LOAD)
         continue;
      start = base + exe_auxv_phdrs[i].p_vaddr;
      if (pc >= start && pc < start + exe_auxv_phdrs[i].p_memsz)
         return 1;
   }
   return 0;
}

static int walk_link_map_list(struct link_map *cur, unsigned long pc,
                              char *buf, size_t buflen)
{
   for (; cur != NULL; cur = cur->l_next) {
      int is_exe = !(cur->l_name && cur->l_name[0]);
      const char *use_name = is_exe ? get_executable_path() : cur->l_name;
      int hit = is_exe ? pc_in_exe(cur->l_addr, pc)
                       : pc_in_object_file(use_name, cur->l_addr, pc);
      if (hit)
         return crash_fmt_lib_offset(buf, buflen, use_name, pc - cur->l_addr);
   }
   return -1;
}

int crash_lib_offset_get_signal_safe(unsigned long pc, char *buf, size_t buflen)
{
   const struct r_debug *rd = &_r_debug;
   int extended = (_r_debug.r_version >= 2);

   while (rd != NULL) {
      if (walk_link_map_list(rd->r_map, pc, buf, buflen) == 0)
         return 0;
      if (!extended)
         break;
      rd = (const struct r_debug *)
               ((const struct r_debug_ext_mirror *) rd)->r_next;
   }
   return -1;
}

void crash_lib_offset_prime(void)
{
   (void) get_executable_path();
   exe_auxv_phdrs = (const ElfW(Phdr) *) getauxval(AT_PHDR);
   exe_auxv_phnum = getauxval(AT_PHNUM);
}

const char *crash_lib_offset_exe_path(void)
{
   return get_executable_path();
}
