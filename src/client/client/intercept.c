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

#include <stdint.h>
#include <limits.h>
#include <assert.h>
#include <string.h>
#include "intercept.h"

struct spindle_binding_t spindle_bindings[] = {
   { "", NULL, NULL }, 
   { "open", (void **) &orig_open, (void *) rtcache_open },
   { "open64", (void **) &orig_open64, (void *) rtcache_open64 },
   { "fopen", (void **) &orig_fopen, (void *) rtcache_fopen },
   { "fopen64", (void **) &orig_fopen64, (void *) rtcache_fopen64 },
   { "close", (void **) &orig_close, (void *) rtcache_close },
   { "stat", (void **) &orig_stat, (void *) rtcache_stat },
   { "lstat", (void **) &orig_lstat, (void *) rtcache_lstat },
   { "__xstat", (void **) &orig_xstat, (void *) rtcache_xstat },
   { "__xstat64", (void **) &orig_xstat64, (void *) rtcache_xstat64 },
   { "__lxstat", (void **) &orig_lxstat, (void *) rtcache_lxstat },
   { "__lxstat64", (void **) &orig_lxstat64, (void *) rtcache_lxstat64 },
   { "fstat", (void **) &orig_fstat, (void *) rtcache_fstat },
   { "__fxstat", (void **) &orig_fxstat, (void *) rtcache_fxstat },
   { "__fxstat64", (void **) &orig_fxstat64, (void *) rtcache_fxstat64 },
   { "execl", (void **) NULL, (void *) execl_wrapper },
   { "execv", (void **) &orig_execv, (void *) execv_wrapper },
   { "execle", (void **) NULL, (void *) execle_wrapper },
   { "execve", (void **) &orig_execve, (void *) execve_wrapper },
   { "execlp", (void **) NULL, (void *) execlp_wrapper },
   { "execvp", (void **) &orig_execvp, (void *) execvp_wrapper },
   { "vfork", (void **) NULL, (void *) vfork_wrapper },
   { "spindle_enable", NULL, (void *) int_spindle_enable },
   { "spindle_disable", NULL, (void *) int_spindle_disable },
   { "spindle_is_enabled", NULL, (void *) int_spindle_is_enabled },
   { "spindle_is_present", NULL, (void *) int_spindle_is_present },
   { "spindle_open", NULL, (void *) int_spindle_open },
   { "spindle_stat", NULL, (void *) int_spindle_stat },
   { "spindle_lstat", NULL, (void *) int_spindle_lstat },
   { "spindle_fopen", NULL, (void *) int_spindle_fopen },
   { NULL, NULL, NULL }
};

#define MAX_BINDING_STR_SIZE 19
#define HASH_TABLE_SIZE 128

static int binding_hash_table[HASH_TABLE_SIZE];

static unsigned int hash_func(const char *str) {
   unsigned int hash = 5381, c, len = 0;
   while ((c = *str++)) {
      hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
      if (++len > MAX_BINDING_STR_SIZE) {
         return UINT_MAX;
      }
   }
   return hash % HASH_TABLE_SIZE;
}

void init_bindings_hash()
{
   struct spindle_binding_t *b;
   int cur = 0;

   assert((sizeof(spindle_bindings) / sizeof(*spindle_bindings)) < HASH_TABLE_SIZE);
   for (b = spindle_bindings; b->name != NULL; b++, cur++) {
      int len = (int) strlen(b->name);
      if (!len)
         continue;

      unsigned int pos = hash_func(b->name);
      assert(pos != UINT_MAX);

      while (binding_hash_table[pos] != 0) {
         pos++;
         if (pos == HASH_TABLE_SIZE)
            pos = 0;
      }
      binding_hash_table[pos] = cur;
   }
}

struct spindle_binding_t *lookup_in_binding_hash(const char *name)
{
   struct spindle_binding_t *entry;
   unsigned int pos = hash_func(name), idx;
   if (pos == UINT_MAX)
      return NULL;

   while ((idx = binding_hash_table[pos]) != 0) {
      entry = spindle_bindings + idx;
      if (strcmp(name, entry->name) == 0)
         return entry;
      pos++;
      if (pos == HASH_TABLE_SIZE)
         pos = 0;
   }

   return NULL;
}

struct spindle_binding_t *get_bindings()
{
   return spindle_bindings;
}
