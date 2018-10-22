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

#if !defined(AUDITCLIENT_H_)
#define AUDITCLIENT_H_

#define _GNU_SOURCE

#include <elf.h>
#include <link.h>
#include "client.h"

struct link_map *get_linkmap_from_cookie(uintptr_t *cookie);
void patch_on_linkactivity(struct link_map *lmap);
ElfX_Addr client_call_binding(const char *symname, ElfX_Addr symvalue);
struct link_map *get_linkmap_from_cookie(uintptr_t *cookie);

Elf64_Addr doPermanentBinding_idx(struct link_map *map,
                                  unsigned long plt_reloc_idx,
                                  Elf64_Addr target);
Elf64_Addr doPermanentBinding_noidx(uintptr_t *refcook, uintptr_t *defcook,
                                    Elf64_Addr target, const char *symname,
                                    void *stack_begin, void *stack_end);


#define AUDIT_EXPORT __attribute__((__visibility__("default")))

#endif
