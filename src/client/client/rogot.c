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

#include "client.h"
#include "spindle_debug.h"
#include <sys/mman.h>
#include <elf.h>
#include <link.h>

static void remove_lib_rogot(const ElfW(Phdr) *phdrs, int num_phdrs, ElfW(Addr) load_address, const char *libname)
{
   const ElfW(Phdr) *p;
   int i;

   debug_printf3("Checking whether %s has R GOT\n", libname);
   if (!phdrs) {
      debug_printf3("%s has no phdrs.  Aborting R GOT check\n", libname);
      return;
   }

   for (p = phdrs, i = 0; i < num_phdrs; i++, p++) {
      if (p->p_type == PT_GNU_RELRO) {
         ElfW(Addr) size, base;
         base = (p->p_vaddr + load_address) & ~(((ElfW(Addr)) getpagesize())-1);
         size = (p->p_vaddr + load_address + p->p_memsz) - base;
         debug_printf3("Changing %s R GOT to RW GOT from %lx to %lx\n", libname, base, base+size);
         mprotect((void *) base, size, PROT_READ|PROT_WRITE);
      }
   }
}

void remove_libc_rogot()
{
   static int cleaned_libc = 0;
   int num_phdrs;
   const ElfW(Phdr) *phdrs;
   if (cleaned_libc)
      return;

   phdrs = find_libc_phdrs(&num_phdrs);
   if (!phdrs)
      return;

   remove_lib_rogot(phdrs, num_phdrs, find_libc_loadoffset(), find_libc_name());

   phdrs = find_interp_phdrs(&num_phdrs);
   remove_lib_rogot(phdrs, num_phdrs, find_interp_loadoffset(), find_interp_name());

   cleaned_libc = 1;
}

