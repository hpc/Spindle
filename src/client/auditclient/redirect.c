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

#include "intercept.h"
#include "client.h"
#include "spindle_debug.h"

#include <string.h>
#include <elf.h>

ElfX_Addr client_call_binding(const char *symname, ElfX_Addr symvalue)
{
   struct spindle_binding_t *binding;

   if (run_tests && strcmp(symname, "spindle_test_log_msg") == 0)
      return (Elf64_Addr) int_spindle_test_log_msg;
   if (!app_errno_location && strcmp(symname, ERRNO_NAME) == 0) {
      app_errno_location = (errno_location_t) symvalue;
      return symvalue;
   }

   binding = lookup_in_binding_hash(symname);
   if (!binding)
      return symvalue;

   if (*binding->libc_func == NULL)
      *binding->libc_func = (void *) symvalue;
   
   return (ElfX_Addr) binding->spindle_func;
}

