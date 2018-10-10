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


/**
 * When ld.so has LD_AUDIT set it will do huge calloc operations of size 
 * LDSO_EXTRA_CALLOC_SZ * DT_PLTRELSZ for each library.  Because we redirect
 * ld.so's binding operation to not use the audit version, these are unnecessary
 * calloc's that aren't used.  But they can chew up memory (300MB per process on 
 * Pynamic).  
 *
 * We thus overwrite ld.so's calloc() function by writing our own over ld.so's GOT
 * table.  Our calloc watches for allocations of the appropriate sizes and drops
 * them on the ground.
 **/

#define _GNU_SOURCE

#include <stdlib.h>
#include <link.h>
#include <elf.h>
#include <sys/mman.h>
#include <assert.h>

#include "spindle_debug.h"
#include "subaudit.h"
#include "parse_plt.h"
#include "config.h"
#include "client_heap.h"
#include "client.h"

struct link_map *get_ldso()
{
   static struct link_map *result = 0;
   if (result)
      return result;

   char *interp_name = find_interp_name();
   for (result = _r_debug.r_map; result != NULL; result = result->l_next) {
      if (result->l_name && (interp_name == result->l_name ||  strcmp(result->l_name, interp_name) == 0)) {
         return result;
      }
   }
   for (result = _r_debug.r_map; result != NULL; result = result->l_next) {
      if (result->l_name && strstr(result->l_name, "/ld"))
         return result;
   }
   return NULL;
}
