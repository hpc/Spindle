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
#include <link.h>
#include <unistd.h>
#include <sys/mman.h>

#include "spindle_debug.h"
#include "subaudit.h"
#include "client.h"
#include "intercept.h"

unsigned int spindle_la_version(unsigned int version)
{
   int result;
   int binding_offset = 0;

   result = lookup_calloc_got();
   if (result == -1)
      return 0;
   update_calloc_got();

   result = get_ldso_metadata(&binding_offset);
   if (result == -1) {
      err_printf("Unable to lookup binding offset\n");
      return -1;
   }
   debug_printf3("Updating subaudit bindings with offset %d\n", binding_offset);
   init_plt_binding_func(binding_offset);

   init_bindings_hash();

   return 1;
}

static void bind_to_libc()
{
   static int bound_libc_symbols = 0;
   int result;
   if (!bound_libc_symbols) {
      result = lookup_libc_symbols();
      if (result == 0) {
         bound_libc_symbols = 1;
      }
   }
}

void spindle_la_activity(uintptr_t *cookie, unsigned int flag)
{
   bind_to_libc();
   update_calloc_got();
   update_plt_bindings();
}

unsigned int spindle_la_objopen(struct link_map *map, Lmid_t lmid, uintptr_t *cookie)
{
   bind_to_libc();
   add_library_to_plt_update_list(map);
   add_library_to_calloc_list(map);
   return 0;
}

int protect_range(void *address, unsigned long size, int prot)
{
   unsigned long start_page, end_page;
   static unsigned long pagesize;

   if (!pagesize)
      pagesize = getpagesize();

   start_page = ((unsigned long) address) & ~(pagesize-1);

   end_page = (((unsigned long) address) + size);
   if (end_page | (pagesize-1)) {
      end_page &= ~(pagesize-1);
      end_page += pagesize;
   }

   return mprotect((void *) start_page, end_page - start_page, prot);
}
