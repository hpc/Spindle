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

#if !defined(PARSE_PLT_H_)
#define PARSE_PLT_H_

#define _GNU_SOURCE

#include <elf.h>
#include <link.h>

#if __WORDSIZE == 64
#define R_SYM(X) ELF64_R_SYM(X)
#else
#define R_SYM(X) ELF32_R_SYM(X)
#endif

#define INIT_DYNAMIC(lmap)                                      \
   ElfW(Dyn) *dynsec = NULL, *dentry = NULL;                    \
   ElfW(Rela) *rela = NULL;                                     \
   ElfW(Rel) *rel = NULL;                                       \
   ElfW(Addr) jmprel = 0;                                       \
   ElfW(Sym) *symtab = NULL;                                    \
   ElfW(Addr) gnu_hash = 0x0, elf_hash = 0x0;                   \
   ElfW(Addr) got = 0x0;                                        \
   char *strtab = NULL;                                         \
   unsigned int rel_size = 0, rel_count, is_rela = 0, i;        \
   dynsec = lmap->l_ld;                                         \
   if (!dynsec)                                                 \
      return -1;                                                \
   for (dentry = dynsec; dentry->d_tag != DT_NULL; dentry++) {  \
      switch (dentry->d_tag) {                                  \
         case DT_PLTRELSZ: {                                    \
            rel_size = (unsigned int) dentry->d_un.d_val;       \
            break;                                              \
         }                                                      \
         case DT_PLTGOT: {                                      \
            got = dentry->d_un.d_ptr;                           \
            break;                                              \
         }                                                      \
         case DT_HASH: {                                        \
            elf_hash = dentry->d_un.d_val;                      \
            break;                                              \
         }                                                      \
         case DT_STRTAB: {                                      \
            strtab = (char *) dentry->d_un.d_ptr;               \
            break;                                              \
         }                                                      \
         case DT_SYMTAB: {                                      \
            symtab = (ElfW(Sym) *) dentry->d_un.d_ptr;          \
            break;                                              \
         }                                                      \
         case DT_PLTREL: {                                      \
            is_rela = (dentry->d_un.d_val == DT_RELA);          \
            break;                                              \
         }                                                      \
         case DT_JMPREL: {                                      \
            jmprel = dentry->d_un.d_val;                        \
            break;                                              \
         }                                                      \
         case DT_GNU_HASH: {                                    \
            gnu_hash = dentry->d_un.d_val;                      \
            break;                                              \
         }                                                      \
      }                                                         \
   }                                                            \
   (void) rela;                                                 \
   (void) rel;                                                  \
   (void) jmprel;                                               \
   (void) symtab;                                               \
   (void) gnu_hash;                                             \
   (void) elf_hash;                                             \
   (void) got;                                                  \
   (void) strtab;                                               \
   (void) rel_size;                                             \
   (void) rel_count;                                            \
   (void) is_rela;                                              \
   (void) i;

   

#define FOR_EACH_PLTREL_INT(relptr, op)               \
   rel_count = rel_size / sizeof(*relptr);            \
   for (i = 0; i < rel_count; i++) {                  \
      ElfW(Addr) offset = relptr[i].r_offset;         \
      unsigned long symidx = R_SYM(relptr[i].r_info); \
      ElfW(Sym) *sym = symtab + symidx;               \
      char *symname = strtab + sym->st_name;          \
      op(sym, symname, offset);                       \
   }

#define FOR_EACH_PLTREL(lmap, op) {             \
      INIT_DYNAMIC(lmap)                        \
      if (is_rela) {                            \
         rela = (ElfW(Rela) *) jmprel;          \
         FOR_EACH_PLTREL_INT(rela, op);         \
      }                                         \
      else {                                    \
         rel = (ElfW(Rel) *) jmprel;            \
         FOR_EACH_PLTREL_INT(rel, op);          \
      }                                         \
   }


#endif
