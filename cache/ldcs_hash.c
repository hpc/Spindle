/*
This file is part of Spindle.  For copyright information see the COPYRIGHT 
file in the top level directory, or at 
<TODO:URL>.

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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "ldcs_api.h"
#include "ldcs_hash.h"

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
   newentry->next = NULL;
   return;
}

struct ldcs_hash_entry_t *ldcs_hash_updateEntryLocalPath(char *dirname, char *filename, char *localpath) {
   struct ldcs_hash_entry_t *entry;

   debug_printf3("Update localpath dir='%s' fn='%s' to cache %s\n", dirname, filename, localpath);
   entry=ldcs_hash_Lookup_FN_and_DIR(filename, dirname);
   if(!entry) {
      debug_printf3("No key for %s at \n", filename);
      return NULL;
   }   
   if(entry->localpath) free(entry->localpath);
   entry->localpath = (localpath)?strdup(localpath):NULL;
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

struct ldcs_hash_entry_t *ldcs_hash_Lookup_FN_and_Ostate(const char *filename, int ostate) {
   struct ldcs_hash_entry_t *entry;
   ldcs_hash_key_t key = ldcs_hash_Val(filename);
   unsigned int index = (unsigned int) key % HASH_SIZE;
   
   if (ldcs_hash_table[index].dirname == NULL) {
      debug_printf3("No key for %s at index %u\n", filename, index);
      return NULL;
   }

   entry = ldcs_hash_table+index;
   while (entry != NULL) {
      if (entry->hash_val == key && strcmp(filename, entry->filename) == 0
	  && entry->ostate==ostate) {
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

struct ldcs_hash_entry_t *ldcs_hash_Lookup_FN_and_DIR_Ostate(const char *filename, const char *dirname, int ostate) {
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
	 strcmp(dirname, entry->dirname) == 0  && entry->ostate==ostate ) {
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

struct ldcs_hash_entry_t *ldcs_hash_getNextNewEntry() {
  
  while(ldcs_hash_last_new_entry_index<HASH_SIZE) {
    /* debug_printf3("scan entry at %d\n", ldcs_hash_last_new_entry_index); */
    while (ldcs_hash_last_new_entry_ptr != NULL) {
      /* debug_printf3("scan entry at %d ptr=%x\n", ldcs_hash_last_new_entry_index,ldcs_hash_last_new_entry_ptr); */
      if(ldcs_hash_last_new_entry_ptr->state == HASH_ENTRY_STATUS_NEW) {
	ldcs_hash_last_new_entry_ptr->state = HASH_ENTRY_STATUS_USED;
	/* debug_printf3("found entry at %d\n", ldcs_hash_last_new_entry_index); */
	return(ldcs_hash_last_new_entry_ptr);
      }
      ldcs_hash_last_new_entry_ptr = ldcs_hash_last_new_entry_ptr->next;
    }
    ldcs_hash_last_new_entry_index++;
    ldcs_hash_last_new_entry_ptr = ldcs_hash_table+ldcs_hash_last_new_entry_index;
  }

  return(NULL);
}

struct ldcs_hash_entry_t *ldcs_hash_getFirstNewEntry() {
  ldcs_hash_last_new_entry_index=0;
  ldcs_hash_last_new_entry_ptr=ldcs_hash_table+ldcs_hash_last_new_entry_index;

  return(ldcs_hash_getNextNewEntry());
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
  return(rc);
}
