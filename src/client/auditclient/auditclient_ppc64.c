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

static void get_section_info(uintptr_t *cookie, 
                             struct link_map **lmap,
                             Elf64_Rela **rels, 
                             Elf64_Sym **dynsyms, char **dynstr,
                             Elf64_Xword *relsize)
{
   Elf64_Dyn *dynamic_section;

   *lmap = get_linkmap_from_cookie(cookie);
   dynamic_section = (*lmap)->l_ld;

   for (; dynamic_section->d_tag != DT_NULL; dynamic_section++) {
      if (dynamic_section->d_tag == DT_JMPREL) {
         *rels = (Elf64_Rela *) dynamic_section->d_un.d_ptr;
      }
      if (dynamic_section->d_tag == DT_SYMTAB) {
         *dynsyms = (Elf64_Sym *) dynamic_section->d_un.d_ptr;
      }
      if (dynamic_section->d_tag == DT_STRTAB) {
         *dynstr = (char *) dynamic_section->d_un.d_ptr;
      }
      if (dynamic_section->d_tag == DT_PLTRELSZ) {
         *relsize = dynamic_section->d_un.d_val;
      }
   }
}

static int check_sym_index(uint32_t index, const char *symname,
                           Elf64_Rela *rels, char *dynstrs,
                           Elf64_Sym *dynsyms)
{
   Elf64_Rela *reloc = NULL;
   Elf64_Sym *sym;
   char *dyn_symname;
   
   reloc = rels + index;
   sym = dynsyms + ELF64_R_SYM(reloc->r_info);
   
   dyn_symname = dynstrs + sym->st_name;

   return (strcmp(symname, dyn_symname) == 0);
}

                      
static int find_refsymbol_index(uint32_t *begin, uint32_t *end,
                                const char *symname,
                                Elf64_Rela *rels, char *dynstrs,
                                Elf64_Sym *dynsyms, Elf64_Xword relsize,
                                char *objname)
{
   static unsigned int prev_i = 0;
   unsigned int i, size = end - begin, start_i;
   int tested_zero = 0;
   uint32_t num_rels;

   /* Scan the stack for the index value.  Remember where it was so we can
      fast check it on future iterations */
   i = start_i = prev_i;
   do {
      uint32_t index;
      uint32_t val = begin[i];
      if ( (val < relsize) && 
           (!tested_zero || val != 0) && 
           (val % sizeof(*rels) == 0) )
      {
         index = val / sizeof(*rels);
         
         if (check_sym_index(index, symname, rels, dynstrs, dynsyms)) {
            if (prev_i != i) {
               debug_printf("Bound %s in %s at index %d (position %d)\n",
                   symname, objname, (int) index, i);
               prev_i = i;
            }
            return (int) index;
         }

         if (val == 0)
            tested_zero = 1;
      }

      i++;
      if (i == size)
         i = 0;
   } while (i != start_i);

   debug_printf("WARNING - Stack scanning for index failed for "
       "symbol %s in %s.  Testing every relocation\n", symname, objname);

   num_rels = (uint32_t) (relsize / sizeof(*rels));
   for (i = 0; i < num_rels; i++) {
      if (check_sym_index(i, symname, rels, dynstrs, dynsyms)) {
         return (int) i;
      }
   }

   return -1;
}

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
   got_entry[0] = func->fptr;
   got_entry[1] = func->toc;
#else
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
   return doPermanentBinding(refcook, defcook, target, symname,
                             sp, (void *) regs);
}
