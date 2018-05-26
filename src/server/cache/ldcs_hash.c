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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

#include "ldcs_api.h"
#include "ldcs_hash.h"
#include "global_name.h"

struct ldcs_hash_entry_t  ldcs_hash_table[HASH_SIZE];
int                       ldcs_hash_last_new_entry_index=-1;
struct ldcs_hash_entry_t *ldcs_hash_last_new_entry_ptr=NULL;

ldcs_hash_key_t ldcs_hash_Val(const char *str) {
   ldcs_hash_key_t hash = 5381;
   int c;
   while ((c = *str++))
      hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
   return hash;
}

void ldcs_hash_addEntry(char *dirname, char *filename) {
   struct ldcs_hash_entry_t *newentry;
   ldcs_hash_key_t key = ldcs_hash_Val(filename);
   unsigned int index = (unsigned int) key % HASH_SIZE;
   int is_dir = (dirname == filename || strcmp(dirname, filename) == 0);

   /* debug_printf3("Adding dir='%s' fn='%s' to cache at index %u\n", dirname, filename, index); */
   newentry = ldcs_hash_table + index;
   if (newentry->dirname != NULL) {
      while (newentry->next != NULL)
         newentry = newentry->next;
      newentry->next = (struct ldcs_hash_entry_t *) malloc(sizeof(struct ldcs_hash_entry_t));
      newentry = newentry->next;
   }
   newentry->filename = (filename)?strdup(filename):NULL;
   newentry->dirname = (dirname)?strdup(dirname):NULL;
   newentry->hash_val = key;
   newentry->state = HASH_ENTRY_STATUS_NEW;
   newentry->ostate = 0;
   newentry->localpath = NULL;
   newentry->buffer = NULL;
   newentry->buffer_size = 0;
   newentry->next = NULL;

   if (is_dir) {
      newentry->dir_next = NULL;
      return;
   }

   struct ldcs_hash_entry_t *dent = ldcs_hash_Lookup(dirname);
   if (!dent) {
      ldcs_hash_addEntry(dirname, dirname);
      dent = ldcs_hash_Lookup(dirname);
   }
   newentry->dir_next = dent->dir_next;
   dent->dir_next = newentry;

   return;
}

struct ldcs_hash_entry_t *ldcs_hash_updateEntry(char *filename, char *dirname, char *localname, 
                                                void *buffer, size_t buffer_size, int errcode)
{
   struct ldcs_hash_entry_t *entry;

   debug_printf3("Update cache entry dir='%s' fn='%s' ln='%s', buffer='%p', size='%lu', errcode='%d'\n", 
                 dirname, filename, localname, buffer, (unsigned long) buffer_size, errcode);

   entry = ldcs_hash_Lookup_FN_and_DIR(filename, dirname);
   assert(entry);
   entry->localpath = localname;
   entry->buffer = buffer;
   entry->buffer_size = buffer_size;
   entry->errcode = errcode;
   return entry;
}

struct ldcs_hash_entry_t *ldcs_hash_updateEntryOState(char *filename, char *dirname, int ostate) {
   struct ldcs_hash_entry_t *entry;

   debug_printf3("Update ostate=%d fn='%s' to cache\n", ostate, filename);
   entry=ldcs_hash_Lookup_FN_and_DIR(filename, dirname);
   if(!entry) {
      debug_printf3("No key for %s at \n", filename);
      return NULL;
   }   
   
   entry->ostate = ostate;
   entry->state  = HASH_ENTRY_STATUS_NEW;
   return entry;
}

struct ldcs_hash_entry_t *ldcs_hash_Lookup(const char *filename) {
   struct ldcs_hash_entry_t *entry;
   ldcs_hash_key_t key = ldcs_hash_Val(filename);
   unsigned int index = (unsigned int) key % HASH_SIZE;
   
   if (ldcs_hash_table[index].dirname == NULL) {
      debug_printf3("No key for %s at index %u\n", filename, index);
      return NULL;
   }

   entry = ldcs_hash_table+index;
   while (entry != NULL) {
      if (entry->hash_val == key && strcmp(filename, entry->filename) == 0) {
         /* debug_printf3("Found entry for %s\n", filename); */
         return entry;
      }
      entry = entry->next;
   } 
   debug_printf3("No key for %s at index %u\n", filename, index);
   return NULL;
}

struct ldcs_hash_entry_t *ldcs_hash_Lookup_FN_and_DIR(const char *filename, const char *dirname) {
   struct ldcs_hash_entry_t *entry;
   ldcs_hash_key_t key = ldcs_hash_Val(filename);
   unsigned int index = (unsigned int) key % HASH_SIZE;
   
   if (ldcs_hash_table[index].dirname == NULL) {
      debug_printf3("No key for %s at index %u\n", filename, index);
      return NULL;
   }

   entry = ldcs_hash_table+index;
   while (entry != NULL) {
     if (entry->hash_val == key && strcmp(filename, entry->filename) == 0 &&
	 strcmp(dirname, entry->dirname) == 0 ) {
	    /* debug_printf3("Found entry for %s in dir %s\n", filename, dirname); */
       return entry;
     }
     entry = entry->next;
   } 
     debug_printf3("No key for %s in dir %s at index %u\n", filename, dirname,index);
   return NULL;
}

void ldcs_hash_dump(char *tofile) {
  FILE *dumpfile;
  struct ldcs_hash_entry_t *entry;
  int index;
 
  dumpfile=fopen(tofile, "w");
  
  for(index=0;index<HASH_SIZE;index++) {
    entry = ldcs_hash_table+index;
    while (entry != NULL) {
      if(entry->hash_val != 0) {
	fprintf(dumpfile,"%4d: %16u %s %s %s\n",
		index,entry->hash_val,entry->filename,entry->dirname,
		(entry->state == HASH_ENTRY_STATUS_USED)        ? "HASH_ENTRY_STATUS_USED" :
		(entry->state == HASH_ENTRY_STATUS_NEW)         ? "HASH_ENTRY_STATUS_NEW" :
		(entry->state == HASH_ENTRY_STATUS_FREE)        ? "HASH_ENTRY_STATUS_FREE" :
		(entry->state == HASH_ENTRY_STATUS_UNKNOWN)     ? "HASH_ENTRY_STATUS_UNKNOWN" : "???"
		);
      }
      entry = entry->next;
    } 
  }
  fclose(dumpfile);
}

int ldcs_hash_init() {
  int rc=0;
  int index;

  for(index=0;index<HASH_SIZE;index++) {
    ldcs_hash_table[index].hash_val = 0;
    ldcs_hash_table[index].state    = HASH_ENTRY_STATUS_FREE;
    ldcs_hash_table[index].next     = NULL;
    ldcs_hash_table[index].filename = NULL;
    ldcs_hash_table[index].dirname  = NULL;
  }
  init_global_name_list();
  return(rc);
}

struct ldcs_hash_entry_t *ldcs_hash_getFirstEntryForDir(char *dirname)
{
   struct ldcs_hash_entry_t *dent = ldcs_hash_Lookup(dirname);
   if (!dent)
      return NULL;
   return dent->dir_next;
}

struct ldcs_hash_entry_t *ldcs_hash_getNextEntryForDir(struct ldcs_hash_entry_t *prev_entry)
{
   return prev_entry->dir_next;
}
