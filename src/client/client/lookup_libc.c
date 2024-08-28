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

#define _GNU_SOURCE

#include <stdint.h>
#include <link.h>
#include <string.h>
#include <link.h>
#include <elf.h>
#include <assert.h>

#include "parse_plt.h"
#include "spindle_debug.h"
#include "subaudit.h"
#include "intercept.h"
#include "client.h"

extern errno_location_t app_errno_location;

uint32_t gnu_hash_func(const char *str) {
   uint32_t hash = 5381;
   for (; *str != '\0'; str++)
      hash = hash * 33 + *str;
   return hash;
}

struct gnu_hash_header {
   uint32_t nbuckets;
   uint32_t symndx;
   uint32_t maskwords;
   uint32_t shift2;
};

static malloc_sig_t mallocfunc = NULL;

static signed long lookup_gnu_hash_symbol(const char *name, ElfW(Sym) *syms, char *symnames, struct gnu_hash_header *header)
{
   uint32_t *buckets, *vals;
   uint32_t hash_val;
   uint32_t cur_sym, cur_sym_hashval;

   buckets = (uint32_t *) (((unsigned char *) (header+1)) + (header->maskwords * sizeof(ElfW(Addr))));
   vals = buckets + header->nbuckets;
   
   hash_val = gnu_hash_func(name);
   cur_sym = buckets[hash_val % header->nbuckets];
   if (cur_sym == 0)
      return -1;

   hash_val &= ~1;
   for (;;) {
      cur_sym_hashval = vals[cur_sym - header->symndx];
      if (((cur_sym_hashval & ~1) == hash_val) && 
          (strcmp(name, symnames + syms[cur_sym].st_name) == 0))
         return (signed long) cur_sym;
      if (cur_sym_hashval & 1)
         return -1;
      cur_sym++;
   }
}

static unsigned long elf_hash(const unsigned char *name)
{
   unsigned long h = 0, g;
   while (*name) {
      h = (h << 4) + *name++;
      if ((g = h & 0xf0000000))
         h ^= g >> 24;
      h &= ~g;
   }
   return h;
}

static signed long lookup_elf_hash_symbol(const char *name, ElfW(Sym) *syms, char *symnames, ElfW(Word) *header)
{
   ElfW(Word) *nbucket = header + 0;
   /*ElfW(Word) *nchain = header + 1;*/
   ElfW(Word) *buckets = header + 2;
   ElfW(Word) *chains = buckets + *nbucket;
   
   unsigned int hash_idx = elf_hash((const unsigned char *) name) % *nbucket;
   signed long idx = (signed long) buckets[hash_idx];
   while (idx != STN_UNDEF) {
      if (strcmp(name, symnames + syms[idx].st_name) == 0)
         return idx;
      idx = chains[idx];
   }
   
   return -1;
}

static struct link_map *get_libc()
{
   struct link_map *l;
   for (l = _r_debug.r_map; l != NULL; l = l->l_next) {
      debug_printf3("Looking for libc: is it %s?\n", 
            (l->l_name != NULL) ? l->l_name : "No Name");
      if (l->l_name && (strstr(l->l_name, "libc-") || strstr(l->l_name, "libc."))) {
         debug_printf3("libc found %s\n", 
               (l->l_name != NULL) ? l->l_name : "No Name");
         return l;
      }
   }
   return NULL;
}

int lookup_libc_symbols()
{
   struct link_map *libc;
   struct spindle_binding_t *binding;
   signed long result;
   int found = 0, not_found = 0;

   debug_printf("Looking up bindings for spindle intercepted symbols in libc\n");
   libc = get_libc();
   if (!libc) {
      debug_printf3("Could not find libc in link maps.  Postponing libc bindings.\n");
      return -1;
   }

   {
      INIT_DYNAMIC(libc);
      assert(gnu_hash || elf_hash);

      if (opts & OPT_SUBAUDIT) { //These bindings only need manual lookups for subaudit
         for (binding = get_bindings(); binding->name != NULL; binding++) {
            if (binding->name[0] == '\0' || !binding->libc_func)
               continue;
            
            result = -1;
            if (gnu_hash)
               result = lookup_gnu_hash_symbol(binding->name, symtab, strtab, (struct gnu_hash_header *) gnu_hash);
            if (elf_hash && result == -1)
               result = lookup_elf_hash_symbol(binding->name, symtab, strtab, (ElfW(Word) *) elf_hash);
            
            if (result == -1) {
               debug_printf3("Warning, Could not bind symbol %s in libc\n", binding->name);
               *binding->libc_func = NULL;
               not_found++;
            }
            else {
               *binding->libc_func = (void *) (symtab[result].st_value + libc->l_addr);
               found++;
            }
         }
      }

      result = -1;
      if (gnu_hash)
         result = lookup_gnu_hash_symbol("__errno_location", symtab, strtab, (struct gnu_hash_header *) gnu_hash);
      if (elf_hash && result == -1)
         result = lookup_elf_hash_symbol("__errno_location", symtab, strtab, (ElfW(Word) *) elf_hash);
      if (result == -1) {        
         debug_printf3("Warning, Could not bind symbol __errno_location in libc\n");
         not_found++;
      }
      else {
         app_errno_location = (void *) (symtab[result].st_value + libc->l_addr);
         debug_printf3("Bound errno_location to %p\n", app_errno_location);
         found++;
      }

      result = -1;
      if (gnu_hash)
         result = lookup_gnu_hash_symbol("malloc", symtab, strtab, (struct gnu_hash_header *) gnu_hash);
      if (elf_hash && result == -1)
         result = lookup_elf_hash_symbol("malloc", symtab, strtab, (ElfW(Word) *) elf_hash);
      if (result == -1) {        
         debug_printf3("Warning, Could not find symbol malloc in libc\n");
         not_found++;
      }
      else {
         mallocfunc = (malloc_sig_t) (symtab[result].st_value + libc->l_addr);
         debug_printf3("Bound mallocfunc to %p\n", mallocfunc);
         found++;
      }      

   }

   if (!found && not_found) {
      err_printf("Could not bind any symbols in libc.\n");
      return -1;
   }

   return 0;
}

malloc_sig_t get_libc_malloc()
{
   if (mallocfunc)
      return mallocfunc;
   lookup_libc_symbols();
   return mallocfunc;
}
