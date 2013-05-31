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

#include "client.h"
#include "auditclient.h"

Elf64_Addr la_ppc64_gnu_pltenter(Elf64_Sym *sym,
                                 unsigned int ndx,
                                 uintptr_t *refcook,
                                 uintptr_t *defcook,
                                 La_ppc64_regs *regs,
                                 unsigned int *flags,
                                 const char *symname,
                                 long int *framesizep)
{
   struct link_map *map = get_linkmap_from_cookie(refcook);
   Elf64_Addr target = client_call_binding(symname, sym->st_value);
   /* return doPermanentBinding();  */
   return target;
}
