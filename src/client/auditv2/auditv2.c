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

#include "auditclient.h"

#define ONCEPLT 4

unsigned int auditv2_la_version(unsigned int version)
{
   return version;
}

void auditv2_la_activity(uintptr_t *cookie, unsigned int flag)
{
   return;
}

unsigned int auditv2_la_objopen(struct link_map *map, Lmid_t lmid, uintptr_t *cookie)
{
   patch_on_linkactivity(map);
   return LA_FLG_BINDTO | LA_FLG_BINDFROM | ONCEPLT;
}

#include <stdio.h>

Elf64_Addr COMBINE_NAME(auditv2, PLTENTER_NAME)(Elf64_Sym *sym, unsigned int ndx,
                                                uintptr_t *refcook, uintptr_t *defcook,
                                                REGS_TYPE *regs, unsigned int *flags,
                                                const char *symname, long int *framesizep)
{
   static volatile int ready = 0;
   fprintf(stderr, "I am %d.  Ready is %p\n", getpid(), &ready);
   while (!ready) {
      sleep(1);
      }
   fprintf(stderr, "Binding for %s in %d\n", symname ? : "NULL", getpid());
   return client_call_binding(symname, sym->st_value);
}
