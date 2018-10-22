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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdlib.h>
#include "client.h"
#include "auditclient.h"
#include "spindle_debug.h"

#if _CALL_ELF != 2
// v1 ABI
#define SPINDLE_PLTENTER la_ppc64_gnu_pltenter
#define SPINDLE_PPC_REGS La_ppc64_regs

struct ppc64_funcptr_t {
   Elf64_Addr fptr;
   Elf64_Addr toc;
};
#else
// v2 ABI
#define SPINDLE_PLTENTER la_ppc64v2_gnu_pltenter
#define SPINDLE_PPC_REGS La_ppc64v2_regs
#endif

Elf64_Addr SPINDLE_PLTENTER(Elf64_Sym *sym, unsigned int ndx,
                                 uintptr_t *refcook, uintptr_t *defcook,
                                 SPINDLE_PPC_REGS *regs, unsigned int *flags,
                                 const char *symname, long int *framesizep) AUDIT_EXPORT;

static Elf64_Addr doPermanentBinding(uintptr_t *refcook, uintptr_t *defcook,
                                     Elf64_Addr target, const char *symname,
                                     void *stack_begin, void *stack_end)
{
   int plt_reloc_idx;
   Elf64_Rela *rels = NULL, *rel;
   Elf64_Xword relsize = 0;
   Elf64_Sym *dynsyms = NULL;
   char *dynstr = NULL;
   char *objname;
   Elf64_Addr *got_entry;
   Elf64_Addr base;
   struct link_map *rmap;
#if _CALL_ELF != 2
   struct ppc64_funcptr_t *func; 
#endif

   get_section_info(refcook, &rmap, &rels, &dynsyms, &dynstr, &relsize);
   objname = (rmap->l_name && rmap->l_name[0] != '\0') 
                                    ? rmap->l_name : "EXECUTABLE";
   base = rmap->l_addr;

   if (!rels || !dynsyms) {
      err_printf("Object %s does not have proper elf structures\n", objname);
      return target;
   }
   plt_reloc_idx = find_refsymbol_index((uint32_t *) stack_begin, 
                                        (uint32_t *) stack_end, symname, 
                                        rels, dynstr, dynsyms, 
                                        relsize, objname);
   if (plt_reloc_idx == -1) {
      err_printf("Failed to bind symbol %s.  "
                  "All future calls will bounce through Spindle.\n", symname);
      return target;
   }
   rel = rels + plt_reloc_idx;

   got_entry = (Elf64_Addr *) (rel->r_offset + base);

#if _CALL_ELF != 2
   func = (struct ppc64_funcptr_t *) target;
   debug_printf3("%s: Old GOT Entry %p -- New GOT Entry %p\n",
                             symname, (void*)(*got_entry), (void*)func->fptr);
   got_entry[0] = func->fptr;
   got_entry[1] = func->toc;
#else
   debug_printf3("%s: Old GOT Entry %p -- New GOT Entry %p\n",
                             symname, (void*)(*got_entry), (void*)target);
   *got_entry = target;
#endif

   return target;
}

Elf64_Addr SPINDLE_PLTENTER(Elf64_Sym *sym,
                                 unsigned int ndx,
                                 uintptr_t *refcook,
                                 uintptr_t *defcook,
                                 SPINDLE_PPC_REGS *regs,
                                 unsigned int *flags,
                                 const char *symname,
                                 long int *framesizep)
{
   Elf64_Addr target;
   void *sp;

   __asm__("or %0, %%r1, %%r1\n" : "=r" (sp));

   target = client_call_binding(symname, sym->st_value);
   return doPermanentBinding_noidx(refcook, defcook, target, symname,
                                   sp, (void *) regs);
}
