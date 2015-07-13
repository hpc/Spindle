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

#include <link.h>
#include <elf.h>
#include <string.h>

#include "client.h"
#include "intercept.h"


struct libc_binding_t { 
   const char *name;
   void **fptr;
};

static struct libc_binding_t bindings[] = {
   { "stat", (void **) &orig_stat },
   { "lstat", (void **) &orig_lstat },
   { "__xstat", (void **) &orig_xstat },
   { "__lxstat", (void **) &orig_lxstat },
   { "__xstat64", (void **) &orig_xstat64 },
   { "__lxstat64", (void **) &orig_lxstat64 },
   { "fstat", (void **) &orig_fstat },
   { "__fxstat", (void **) &orig_fxstat },
   { "__fxstat64", (void **) &orig_fxstat64 },
   { "execv", (void **) &orig_execv },
   { "execve", (void **) &orig_execve },
   { "execvp", (void **) &orig_execvp },
   { "fork", (void **) &orig_fork },
   { "open", (void **) &orig_open },
   { "open64", (void **) &orig_open64 },
   { "fopen", (void **) &orig_fopen },
   { "fopen64", (void **) &orig_fopen64 },
   { "close", (void **) &orig_close },
   { NULL, NULL } 
};

static struct link_map *find_libc()
{
   struct link_map *map;
   for (map = _r_debug.r_map; map != NULL; map = map->l_next) {
      if (!map->l_name)
         continue;
      if (strcmp(map->l_name, "libc") == 0)
         return map;
   }
   return NULL;
}

void setup_orig_bindings()
{
   (void) bindings;
   (void) find_libc;
}
