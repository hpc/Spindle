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
#include <link.h>
#include <elf.h>
#include <sys/mman.h>
#include <stdlib.h>

#include "subaudit.h"
#include "config.h"
#include "spindle_debug.h"
#include "client_heap.h"
#include "client.h"
#include "parse_plt.h"
#include "intercept.h"

static signed int binding_offset;
static void *dl_runtime_profile_ptr;
static void *dl_runtime_resolve_ptr;

#define GOT_resolve_offset 0

#if defined(arch_x86_64)
typedef Elf64_Addr funcptr_t;
#define ASSIGN_FPTR(TO, FROM) *((Elf64_Addr *) TO) = (Elf64_Addr) FROM
#elif defined(arch_ppc64) || defined(arch_ppc64le)
#if _CALL_ELF != 2
typedef struct {
   Elf64_Addr func;
   Elf64_Addr toc;   
} *funcptr_t;
#define ASSIGN_FPTR(TO, FROM) *((funcptr_t) TO) = *((funcptr_t) FROM)
#else
typedef Elf64_Addr funcptr_t;
#define ASSIGN_FPTR(TO, FROM) *((Elf64_Addr *) TO) = (Elf64_Addr) FROM
#endif
#else
#error Unknown architecture
#endif

void init_plt_binding_func(signed int binding_offset_)
{
   binding_offset = binding_offset_;
   dl_runtime_profile_ptr = NULL;
   dl_runtime_resolve_ptr = NULL;
}

#define UPDATE_ERR -1
#define UPDATE_AGAIN -2
#define UPDATE_DONE -3

static int update_plt_binding_func(struct link_map *lmap)
{
   ElfW(Addr) got = 0;
   ElfW(Dyn) *dynsec, *dentry;
   void **ldso_ptr;
   const char *name = (lmap->l_name && lmap->l_name[0]) ? lmap->l_name : "[NO NAME]";
      
   dynsec = (ElfW(Dyn) *) lmap->l_ld;
   if (!dynsec) {
      err_printf("Could not find dynamic table in link map %s\n", name);
      return UPDATE_ERR;
   }

   for (dentry = dynsec; dentry->d_tag != DT_NULL; dentry++) {
      if (dentry->d_tag == DT_PLTGOT) {
         got = dentry->d_un.d_ptr;
         break;
      }
   }
   if (!got) {
      err_printf("Could not find GOT in link map %s\n", name);
      return UPDATE_ERR;
   }

   debug_printf3("Have GOT for %s at 0x%lx, for binding function update to 0x%lx\n", 
                name, got - lmap->l_addr, got+GOT_resolve_offset);
   ldso_ptr = ((void **) (got+GOT_resolve_offset));
   if (*ldso_ptr == NULL) {
      debug_printf3("Entry %s does not yet have a fixed-up GOT at %p.  Will retry update.\n", name, ldso_ptr);
      return UPDATE_AGAIN;
   }

   if (!dl_runtime_profile_ptr) {
      dl_runtime_profile_ptr = *ldso_ptr;
      dl_runtime_resolve_ptr = (void *) (((ElfW(Addr)) dl_runtime_profile_ptr) + binding_offset);
   }

   if (ldso_ptr == dl_runtime_resolve_ptr) {
      debug_printf3("ld.so pointer for %s already pointed at dl_runtime_resolve\n", name);
      return UPDATE_DONE;
   }

   debug_printf3("Changing ld.so pointer for %s at %p (orig %p) from dl_runtime_profile (%p) to dl_runtime_resolve (%p)\n",
                 name, ldso_ptr, *ldso_ptr, dl_runtime_profile_ptr, dl_runtime_resolve_ptr);
   protect_range(ldso_ptr, sizeof(*ldso_ptr), PROT_READ | PROT_WRITE);
   *ldso_ptr = dl_runtime_resolve_ptr;

   return 0;
}


static struct link_map **update_list = NULL;
static unsigned int update_list_cur = 0;
static unsigned int update_list_size = 0;
#define update_list_initial_size 16

static void grow_update_list()
{
  if (update_list == NULL) {
     update_list_size = update_list_initial_size;
     update_list_cur = 0;
     update_list = spindle_malloc(sizeof(*update_list) * update_list_size);
     return;
  }

  if (update_list_cur + 1 >= update_list_size) {
     update_list_size *= 2;
     update_list = spindle_realloc(update_list, sizeof(*update_list) * update_list_size);
  }
}

static int redirect_interceptions(struct link_map *lmap)
{
   struct spindle_binding_t *binding;
   void **addr;

#define LOOKUP_IN_HASH(SYM, NAME, OFFSET) {                             \
      binding = lookup_in_binding_hash(NAME);                           \
      if (binding) {                                                    \
         addr = (void **) (OFFSET + lmap->l_addr);                      \
         ASSIGN_FPTR(addr, binding->spindle_func);                      \
      }                                                                 \
   }

   FOR_EACH_PLTREL(lmap, LOOKUP_IN_HASH);
   
   return 0;
}

static int redirect_libspindleint_interceptions(struct link_map *lmap)
{
   struct spindle_binding_t *binding, *base;
   void **addr;

   base = get_bindings();
   if (!lmap->l_name || !strstr(lmap->l_name, "libspindleint.so")) {
      return 0;
   }

   debug_printf2("Remapping internal symbols in %s to point to spindle implementations\n", lmap->l_name);

#define LOOKUP_IN_INT_HASH(SYM, NAME, OFFSET) {                   \
      if (SYM->st_shndx != SHN_UNDEF)                             \
         continue;                                                \
      for (binding = base; binding->name != NULL; binding++) {    \
         if (strcmp(binding->spindle_name, NAME) == 0) {          \
            debug_printf3("Mapping %s in library %s to %s\n", NAME, lmap->l_name, binding->name); \
            addr = (void **) (OFFSET + lmap->l_addr);             \
            ASSIGN_FPTR(addr, binding->spindle_func);             \
            break;                                                \
         }                                                        \
      }                                                           \
   }

   FOR_EACH_PLTREL(lmap, LOOKUP_IN_INT_HASH);

   return 0;
}

extern struct link_map *get_ldso();   
void add_library_to_plt_update_list(struct link_map *lmap)
{
   if (!lmap->l_name)
      return;
   if (lmap == get_ldso())
      return;

   grow_update_list();
   update_list[update_list_cur++] = lmap;
}

void remove_library_from_plt_update_list(struct link_map *lmap)
{
   int i, found = -1;
   if (!update_list_cur)
      return;

   for (i = 0; i < update_list_cur; i++) {
      if (lmap == update_list[i]) {
         found = i;
         break;
      }
   }

   debug_printf3("Removing library from update list\n");
   update_list[found] = update_list[update_list_cur-1];
   update_list_cur--;
}

int update_plt_bindings()
{
   unsigned int i, j = 0;
   int result, ret = 0;
   char *ld_preload;
   static enum {
      spindleint_unset,
      spindleint_none,
      spindleint_present
   } has_spindleint = spindleint_unset;

   if (has_spindleint == spindleint_unset) {
      ld_preload = getenv("LD_PRELOAD");
      has_spindleint = ld_preload && strstr(ld_preload, "libspindleint.so") ? spindleint_present : spindleint_unset;
   }


   for (i = 0; i < update_list_cur; i++) {
      result = update_plt_binding_func(update_list[i]);
      if (result == UPDATE_ERR) {
         ret = -1;
         continue;
      }
      else if (result == UPDATE_AGAIN) {
         update_list[j++] = update_list[i];
         continue;
      }
      else if (result == UPDATE_DONE) {
         continue;
      }
      
      if (has_spindleint == spindleint_none)
         redirect_interceptions(update_list[i]);
      else if (has_spindleint == spindleint_present)
         redirect_libspindleint_interceptions(update_list[i]);
   }

   if (j != 0) {
      debug_printf3("Unable to update plt bindings of %u / %u libraries.  Will try these again\n", j, i);
      update_list_cur = j;
      return ret;
   }

   debug_printf3("Clearing library PLT update list\n");
   update_list_cur = 0;

   if (update_list_size >= 64) {
      debug_printf3("Clearing large (%u) sized update list\n", update_list_size);
      update_list_size = 0;
      spindle_free(update_list);
      update_list = NULL;
   }

   return ret;
}
