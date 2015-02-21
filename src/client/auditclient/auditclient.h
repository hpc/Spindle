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

#if !defined(AUDITCLIENT_H_)
#define AUDITCLIENT_H_

#define _GNU_SOURCE

#include <elf.h>
#include <link.h>
#include <stdlib.h>
#include <unistd.h>
#include "client.h"
#include "config.h"

#if defined(arch_x86_64)
#define PLTENTER_NAME la_x86_64_gnu_pltenter
#define REGS_TYPE La_x86_64_regs
#elif defined(arch_ppc64)
#define PLTENTER_NAME la_ppc64_gnu_pltenter
#define REGS_TYPE La_ppc64_regs
#else
#error Unknown architecture
#endif
#define PASTE(PREFIX, NAME) PREFIX ## _ ## NAME
#define COMBINE_NAME(PREFIX, NAME) PASTE(PREFIX, NAME)

#define AUDIT_EXPORT __attribute__((__visibility__("default")))

struct link_map *get_linkmap_from_cookie(uintptr_t *cookie);
void patch_on_linkactivity(struct link_map *lmap);
ElfX_Addr client_call_binding(const char *symname, ElfX_Addr symvalue);

unsigned int auditv1_la_version(unsigned int version);
unsigned int subaudit_la_version(unsigned int version);

void subaudit_la_activity(uintptr_t *cookie, unsigned int flag);
void auditv1_la_activity(uintptr_t *cookie, unsigned int flag);

unsigned int subaudit_la_objopen(struct link_map *map, Lmid_t lmid, uintptr_t *cookie);
unsigned int auditv1_la_objopen(struct link_map *map, Lmid_t lmid, uintptr_t *cookie);

Elf64_Addr COMBINE_NAME(auditv1, PLTENTER_NAME)(Elf64_Sym *sym, unsigned int ndx,
                                                uintptr_t *refcook, uintptr_t *defcook,
                                                REGS_TYPE *regs, unsigned int *flags,
                                                const char *symname, long int *framesizep);
Elf64_Addr COMBINE_NAME(subaudit, PLTENTER_NAME)(Elf64_Sym *sym, unsigned int ndx,
                                                 uintptr_t *refcook, uintptr_t *defcook,
                                                 REGS_TYPE *regs, unsigned int *flags,
                                                 const char *symname, long int *framesizep);

#endif
