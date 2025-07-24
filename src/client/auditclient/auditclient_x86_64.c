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

#include "auditclient.h"
#include "spindle_debug.h"
#include "writablegot.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>

/*Elf64_Addr la_x86_64_gnu_pltenter(Elf64_Sym *sym, unsigned int ndx,
                                  uintptr_t *refcook, uintptr_t *defcook,
                                  La_x86_64_regs *regs, unsigned int *flags,
                                  const char *symname, long int *framesizep) AUDIT_EXPORT;*/

uintptr_t la_symbind64(Elf64_Sym *sym, unsigned int ndx,
                       uintptr_t *refcook, uintptr_t *defcook,
                       unsigned int *flags, const char *symname) AUDIT_EXPORT;

Elf64_Addr la_x86_64_gnu_pltenter(Elf64_Sym *sym,
                                  unsigned int ndx,
                                  uintptr_t *refcook,
                                  uintptr_t *defcook,
                                  La_x86_64_regs *regs,
                                  unsigned int *flags,
                                  const char *symname,
                                  long int *framesizep)
{
   struct link_map *map = get_linkmap_from_cookie(refcook);
   unsigned long reloc_index = *((unsigned long *) (regs->lr_rsp-8));
   Elf64_Addr target = client_call_binding(symname, sym->st_value);
   return doPermanentBinding_idx(map, reloc_index, target, symname);
}

uintptr_t la_symbind64(Elf64_Sym *sym, unsigned int ndx,
                       uintptr_t *refcook, uintptr_t *defcook,
                       unsigned int *flags, const char *symname)
{
//   struct link_map *rmap = get_linkmap_from_cookie(refcook);
//   struct link_map *dmap = get_linkmap_from_cookie(defcook);
   updateDataBindingQueue(0);
   Elf64_Addr target = client_call_binding(symname, sym->st_value);
   *flags = 0;
   return target;
}

static int bind_global_relocations(struct link_map *map, Elf64_Rela *relocs, Elf64_Addr base, unsigned long num_relocs,
                                   Elf64_Sym *symtable, const char *strtable, int has_pltrelocs)
{
   unsigned long i;
   unsigned long symidx;
   Elf64_Addr symvalue, target;
   Elf64_Sym *sym;
   const char *symname;
   Elf64_Addr *got_entry;
   int made_reloc_table_writable = 0, result, error;
   Elf64_Addr reloc_table_pagebase = 0;
   size_t reloc_table_size = 0;

   for (i = 0; i < num_relocs; i++) {
      Elf64_Rela *r = relocs+i;
      if (ELF64_R_TYPE(r->r_info) != R_X86_64_GLOB_DAT)
         continue;      
      symidx = ELF64_R_SYM(r->r_info);
      sym = symtable + symidx;
      symname = strtable + sym->st_name;
      symvalue = sym->st_value;
      target = client_call_binding(symname, symvalue);
      if (target == symvalue)
         continue;
      got_entry = (Elf64_Addr *) (r->r_offset + base);
      if (has_pltrelocs) {
         //Queue up any data-type symbols that need to be relocated and apply them
         // when we do the function-type symbols in la_symbind64
         addToDataBindingQueue(map, target, got_entry);
      }
      else {
         //This DSO has no function-type relocations. So we won't get a symbind callback.
         //We'll do the bindings for the data-type relocations now. To prevent ld.so from
         //overwriting these later, we'll also change the relocation type to make it a NULL
         //operation. This has the unfortunate side effect of modifying the binary, which
         //some programs may notice if they checksum themselves. It's a trade-off between
         //this or messing up data-variable bindings.
         //Spindle's libsymbind_g.so test triggers this.
         debug_printf2("Unusal, have to do data symbol bindings in a DSO without PLT\n");
         make_got_writable(got_entry, map);
         *got_entry = target;
         if (!made_reloc_table_writable) {
            reloc_table_pagebase = ((Elf64_Addr) relocs) & ~((Elf64_Addr) (getpagesize()-1));
            reloc_table_size = ((num_relocs + 1) * sizeof(Elf64_Rela)) - 1 + (((Elf64_Addr) relocs) - reloc_table_pagebase);
            debug_printf3("mprotect(%p, %lu, PROT_READ|PROT_WRITE|PROT_EXEC) making GOT table writiable\n", (void *) reloc_table_pagebase, reloc_table_size);
            result = mprotect((void *) reloc_table_pagebase, reloc_table_size, PROT_READ | PROT_WRITE | PROT_EXEC);
            if (result == -1) {
               error = errno;
               err_printf("Could not mprotect for write to relocs 0x%lx +%lu: %s\n",
                          reloc_table_pagebase, reloc_table_size, strerror(error));
               continue;
            }
            made_reloc_table_writable = 1;
         }
         
         r->r_info &= ~((Elf64_Xword) 0xffffffff);
         r->r_info |= R_X86_64_NONE;
      }
   }

   return 0;
}

int do_global_bindings(struct link_map *map)
{
   Elf64_Dyn *dyn;
   Elf64_Rela *rels = NULL;
   unsigned long num_rels = 0;
   Elf64_Sym *syms = NULL;
   const char *strtable = NULL;
   int has_pltrelocs = 0;

   dyn = map->l_ld;
   if (!dyn) {
      debug_printf2("DSO %s does not have dynamic section\n", map->l_name);
      return 0;
   }
   for (; dyn->d_tag != DT_NULL; dyn++) {
      if (dyn->d_tag == DT_SYMTAB) 
         syms = (Elf64_Sym *) dyn->d_un.d_ptr;
      else if (dyn->d_tag == DT_STRTAB)
         strtable = (char *) dyn->d_un.d_ptr;
      else if (dyn->d_tag == DT_RELA)
         rels = (Elf64_Rela *) dyn->d_un.d_ptr;
      else if (dyn->d_tag == DT_RELASZ)
         num_rels = dyn->d_un.d_val / sizeof(Elf64_Rela);
      else if (dyn->d_tag == DT_JMPREL)
         has_pltrelocs = 1;
   }
   if (!syms || !strtable || !rels || !num_rels) {
      debug_printf2("DSO %s is missing sections needed for variable binding (%p %p %p %lu)\n", map->l_name,
                    syms, strtable, rels, num_rels);
      return 0;
   }

   return bind_global_relocations(map, rels, map->l_addr, num_rels, syms, strtable, has_pltrelocs);
}
