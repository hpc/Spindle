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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "spindle_debug.h"

#define STAT_TABLE_SIZE 1024

typedef struct stat_entry_t
{
   char *pathname;
   char *data;
   unsigned int hash_value;
   struct stat_entry_t *next;
} stat_entry_t;

static unsigned int hashkey(char *str) {
   unsigned int hash = 5381;
   int c;
   while ((c = *str++))
      hash = ((hash << 5) + hash) + c;
   return hash;
}

stat_entry_t *stat_table[STAT_TABLE_SIZE];

int init_stat_cache()
{
   return 0;
}

void add_stat_cache(char *pathname, char *data)
{
   unsigned int key;
   stat_entry_t *newentry;

   debug_printf3("Adding stat cache entry %s = %s\n", pathname);

   key = hashkey(pathname) % STAT_TABLE_SIZE;

   newentry = (stat_entry_t *) malloc(sizeof(stat_entry_t));
   newentry->pathname = strdup(pathname);
   newentry->data = data;
   newentry->hash_value = key;
   newentry->next = stat_table[key];

   stat_table[key] = newentry;
}

int lookup_stat_cache(char *pathname, char **data)
{
   unsigned int key;
   stat_entry_t *entry;

   key = hashkey(pathname) % STAT_TABLE_SIZE;
   entry = stat_table[key];

   for (;;) {      
      if (!entry) {
         *data = NULL;
         debug_printf3("Looked up stat cache entry %s, not cached\n", pathname);
         return -1;
      }
      if (entry->hash_value != key || strcmp(entry->pathname, pathname) != 0) {
         entry = entry->next;
         continue;
      }
      *data = entry->data;
      debug_printf3("Looked up stat entry %s\n");
      return 0;
   }
}
