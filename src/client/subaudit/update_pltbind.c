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

#define _GNU_SOURCE
#include <link.h>
#include <elf.h>
#include <sys/mman.h>

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

#if defined(arch_x86_64)
#define GOT_resolve_offset 16
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
   const char *name = lmap->l_name && lmap->l_name[0] ? lmap->l_name : "[NO NAME]";
      
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

   ldso_ptr = ((void **) (got+GOT_resolve_offset));
   if (*ldso_ptr == NULL) {
      debug_printf("Entry %s does not yet have an fixed-up GOT.  Postponing update.\n", name);
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

#define LOOKUP_IN_HASH(SYM, NAME, OFFSET) {             \
      binding = lookup_in_binding_hash(NAME);           \
      if (binding) {                                    \
         addr = (void **) (OFFSET + lmap->l_addr);      \
         *addr = binding->spindle_func;                 \
      }                                                 \
   }
   
   FOR_EACH_PLTREL(lmap, LOOKUP_IN_HASH);
   
   return 0;
}
   
void add_library_to_plt_update_list(struct link_map *lmap)
{
   if (!lmap->l_name || strstr(lmap->l_name, "/ld.") || strstr(lmap->l_name, "/ld-"))
      return;
   grow_update_list();
   update_list[update_list_cur++] = lmap;
}

int update_plt_bindings()
{
   unsigned int i, j = 0;
   int result, ret = 0;

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

      redirect_interceptions(update_list[i]);
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
