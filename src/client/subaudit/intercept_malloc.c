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


/**
 * When ld.so has LD_AUDIT set it will do huge calloc operations of size 
 * LDSO_EXTRA_CALLOC_SZ * DT_PLTRELSZ for each library.  Because we redirect
 * ld.so's binding operation to not use the audit version, these are unnecessary
 * calloc's that aren't used.  But they can chew up memory (300MB per process on 
 * Pynamic).  
 *
 * We thus overwrite ld.so's calloc() function by writing our own over ld.so's GOT
 * table.  Our calloc watches for allocations of the appropriate sizes and drops
 * them on the ground.
 **/

#define _GNU_SOURCE

#include <stdlib.h>
#include <link.h>
#include <elf.h>
#include <sys/mman.h>
#include <assert.h>

#include "spindle_debug.h"
#include "subaudit.h"
#include "parse_plt.h"
#include "config.h"
#include "client_heap.h"

typedef void *(*malloc_fptr)(size_t);
typedef void *(*calloc_fptr)(size_t, size_t);

static calloc_fptr *calloc_got = NULL;
static calloc_fptr orig_calloc = NULL;

static unsigned int *pltrelsz_list = NULL;
static unsigned int pltrelsz_size = 0, pltrelsz_cur = 0;
#define default_pltrelsz 16

#if defined(arch_x86_64)
#define LDSO_EXTRA_CALLOC_SZ 32
#else
#error Unknown architecture
#endif

static void rm_from_pltrelsz_list(unsigned int idx);

static struct link_map *get_ldso()
{
   struct link_map *result;
   for (result = _r_debug.r_map; result != NULL; result = result->l_next) {
      if (result->l_name && strstr(result->l_name, "/ld")) {
         return result;
      }
   }
   return NULL;
}

int lookup_calloc_got()
{
   struct link_map *ldso;
   ldso = get_ldso();
   if (!ldso) {
      err_printf("Could not find ld.so in link maps\n");
      return -1;
   }

#define ON_CALLOC(SYM, NAME, OFFSET)                                    \
   if (strcmp(symname, "calloc") == 0) {                                \
      calloc_got = (calloc_fptr*) (OFFSET + ldso->l_addr);              \
      debug_printf3("Located ld.so calloc got at %p\n", calloc_got);    \
      return 0;                                                         \
   }

   FOR_EACH_PLTREL(ldso, ON_CALLOC);

   err_printf("Could not find calloc in ld.so PLT\n");
   return -1;
}

void *spindle_ldso_calloc(size_t nmemb, size_t size)
{
   unsigned int i;

   if (nmemb == LDSO_EXTRA_CALLOC_SZ && size >= nmemb) {
      for (i = 0; i < pltrelsz_cur; i++) {
         if (pltrelsz_list[i] == size) {
            rm_from_pltrelsz_list(i);
            debug_printf3("Dropping ld.so calloc(%lu, %lu)\n", nmemb, size);
            return (void *) 0x1;
         }
      }
   }

   void *result = malloc(nmemb * size);
   memset(result, 0, nmemb * size);
   return result;
}

void update_calloc_got()
{
   if (!calloc_got)
      return;
   if (*calloc_got == spindle_ldso_calloc)
      return;

   debug_printf3("Updating calloc pointer in ld.so at %p to be our calloc.  Was pointing at %p\n",
                 calloc_got, *calloc_got);
   protect_range(calloc_got, sizeof(ElfW(Addr)), PROT_READ | PROT_WRITE);
   orig_calloc = *calloc_got;
   *calloc_got = spindle_ldso_calloc;
}

static void grow_pltrelsz_list()
{
   if (!pltrelsz_list) {
      pltrelsz_size = default_pltrelsz;
      pltrelsz_cur = 0;
      pltrelsz_list = spindle_malloc(sizeof(*pltrelsz_list) * pltrelsz_size);
      return;
   }

   pltrelsz_size *= 2;
   pltrelsz_list = spindle_realloc(pltrelsz_list, pltrelsz_size);
}

static void rm_from_pltrelsz_list(unsigned int idx)
{
   assert(idx < pltrelsz_cur);
   if (pltrelsz_cur == 1) {
      spindle_free(pltrelsz_list);
      pltrelsz_list = NULL;
      pltrelsz_cur = 0;
      pltrelsz_size = 0;
      return;
   }

   pltrelsz_cur--;
   pltrelsz_list[idx] = pltrelsz_list[pltrelsz_cur];
}

static unsigned int get_pltrelsz(struct link_map *lmap)
{
   INIT_DYNAMIC(lmap);
   return (unsigned int) rel_size;
}

void add_library_to_calloc_list(struct link_map *lmap)
{
   unsigned int pltrelsz;
   if (!pltrelsz_list || pltrelsz_cur == pltrelsz_size)
      grow_pltrelsz_list();

   pltrelsz = get_pltrelsz(lmap);
   if (pltrelsz)
      pltrelsz_list[pltrelsz_cur++] = pltrelsz;
}

