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

static Elf64_Addr getTOCValue(struct link_map *map)
{
   //Get ELF Header
   //Get TOC from e_entry

   //Or is target a proper PPC64 indirect function pointer?
}

static Elf64_Addr doPermanentBinding(struct link_map *rmap,
                                     struct link_map *dmap,
                                     unsigned long plt_reloc_idx,
                                     Elf64_Addr target)
{
   Elf64_Dyn *dynamic_section = rmap->l_ld;
   Elf64_Rela *rel = NULL;
   Elf64_Addr *got_entry;
   Elf64_Addr base = rmap->l_addr;
   for (; dynamic_section->d_tag != DT_NULL; dynamic_section++) {
      if (dynamic_section->d_tag == DT_JMPREL) {
         rel = ((Elf64_Rela *) dynamic_section->d_un.d_ptr) + plt_reloc_idx;
         break;
      }
   }
   if (!rel)
      return target;
   got_entry = (Elf64_Addr *) (rel->r_offset + base);
   got_entry[0] = target;
   got_entry[1] = ;
   return target;
}

Elf64_Addr la_ppc64_gnu_pltenter(Elf64_Sym *sym,
                                 unsigned int ndx,
                                 uintptr_t *refcook,
                                 uintptr_t *defcook,
                                 La_ppc64_regs *regs,
                                 unsigned int *flags,
                                 const char *symname,
                                 long int *framesizep)
{
   struct link_map *rmap = get_linkmap_from_cookie(refcook);
   struct link_map *dmap = get_linkmap_from_cookie(defcook);
   unsigned long reloc_index = regs->lr_reg[0];
   Elf64_Addr target = client_call_binding(symname, sym->st_value);
   return doPermanentBinding(rmap, dmap, reloc_index, target);
}
