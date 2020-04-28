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
#include <sys/types.h>
#include <dirent.h>

#include <stddef.h>

#include "ldcs_api.h"
#include "ldcs_cache.h"
#include "ldcs_hash.h"

ldcs_cache_result_t ldcs_cache_findDirInCache(char *dirname) {
   struct ldcs_hash_entry_t *e = ldcs_hash_Lookup(dirname);
   if(e) {
     debug_printf3("directory entry exists %d '%s' '%s'\n", e->dirname == e->filename, e->dirname, e->filename);
     return (strcmp(e->dirname, e->filename) == 0) ? 
        LDCS_CACHE_DIR_PARSED_AND_EXISTS : 
        LDCS_CACHE_DIR_PARSED_AND_NOT_EXISTS;
   } else {
     return LDCS_CACHE_DIR_NOT_PARSED;
   }
}

ldcs_cache_result_t ldcs_cache_findFileDirInCache(char *filename, char *dirname, char **localpath, int *errcode) {
  struct ldcs_hash_entry_t *e = ldcs_hash_Lookup_FN_and_DIR(filename, dirname);
   if (e) {
     if(e->ostate==LDCS_CACHE_OBJECT_STATUS_LOCAL_PATH) {
       *localpath = e->localpath;
       *errcode = e->errcode;
     } else {
       *localpath = NULL;
       *errcode = 0;
     }
     return(LDCS_CACHE_FILE_FOUND);
   } else {
      *localpath = NULL;
      *errcode = 0;
     return(LDCS_CACHE_FILE_NOT_FOUND);
   }
}

ldcs_cache_result_t ldcs_cache_getAlias(char *filename, char *dirname, char **alias)
{
  struct ldcs_hash_entry_t *e = ldcs_hash_Lookup_FN_and_DIR(filename, dirname);
  if (e) {
     *alias = e->alias_to;
     return LDCS_CACHE_FILE_FOUND;
  }
  *alias = NULL;
  return LDCS_CACHE_FILE_NOT_FOUND;
}

ldcs_cache_result_t ldcs_cache_processDirectory(char *dirname, size_t *bytesread) {
  if (bytesread) *bytesread = 0;
  debug_printf3("Processing directory %s\n", dirname);
  if (directoryParsed(dirname)) {
    debug_printf3("Directory %s already parsed\n", dirname);
    return(LDCS_CACHE_DIR_PARSED_AND_EXISTS);
  }
  cacheLibraries(dirname, bytesread);
  
  return(ldcs_cache_findDirInCache(dirname));
}

ldcs_cache_result_t ldcs_cache_updateEntry(char *filename, char *dirname, 
                                           char *localname, void *buffer, size_t buffer_size, char *alias_to, int errcode)
{
   struct ldcs_hash_entry_t *e = ldcs_hash_updateEntry(filename, dirname, localname, buffer, buffer_size, alias_to, errcode);
   if(e) { 
      e->ostate = LDCS_CACHE_OBJECT_STATUS_LOCAL_PATH;
      return(LDCS_CACHE_FILE_FOUND);
   }
   else
      return(LDCS_CACHE_FILE_NOT_FOUND);
}

ldcs_cache_result_t ldcs_cache_updateStatus(char *filename, char *dirname, ldcs_hash_object_status_t ostate) {
  struct ldcs_hash_entry_t *e = ldcs_hash_updateEntryOState(filename, dirname, (int) ostate);
  if(e) {     return(LDCS_CACHE_FILE_FOUND);   } 
  else  {    return(LDCS_CACHE_FILE_NOT_FOUND); }
}

ldcs_hash_object_status_t ldcs_cache_getStatus(char *filename) {
  struct ldcs_hash_entry_t *e = ldcs_hash_Lookup(filename);
  if(e) {     return((ldcs_hash_object_status_t) e->ostate);   } 
  else  {    return(LDCS_CACHE_OBJECT_STATUS_UNKNOWN); }
}

int ldcs_cache_get_buffer(char *dirname, char *filename, void **buffer, size_t *size, char **alias_to)
{
   struct ldcs_hash_entry_t *e = ldcs_hash_Lookup_FN_and_DIR(filename, dirname);
   if (!e) {
      return -1;
   }

   *buffer = e->buffer;
   *size = e->buffer_size;
   if (alias_to)
      *alias_to = e->alias_to;
   
   return 0;
}

#define INITIAL_BUFFER_SIZE 4096
int ldcs_cache_getNewEntriesForDir(char *dir, char **data, int *len)
{
   char *buffer = NULL;
   size_t buffer_size = 0;
   size_t cur_pos = 0;
   int first_pass = 1;
   int num_entries = 0;
   struct ldcs_hash_entry_t *i;
   
   for (i = ldcs_hash_getFirstEntryForDir(dir); i != NULL; i = ldcs_hash_getNextEntryForDir(i)) {
      int length_fn = i->filename ? strlen(i->filename)+1 : 0;
      int length_dir = first_pass ? strlen(dir)+1 : 0;
      unsigned int space_needed = length_fn + length_dir + sizeof(int)*2;
      if (cur_pos + space_needed >= buffer_size) {
         while (cur_pos + space_needed >= buffer_size)
            buffer_size = !buffer_size ? INITIAL_BUFFER_SIZE : (buffer_size*2);
         buffer = realloc(buffer, buffer_size);
      }

      memcpy(buffer + cur_pos, &length_fn, sizeof(int));
      cur_pos += sizeof(int);
      if (length_fn)
         strncpy(buffer + cur_pos, i->filename, length_fn);
      cur_pos += length_fn;

      memcpy(buffer + cur_pos, &length_dir, sizeof(int));
      cur_pos += sizeof(int);
      if (length_dir)
         strncpy(buffer + cur_pos, dir, length_dir);
      cur_pos += length_dir;

      first_pass = 0;
      num_entries++;
   }

   if (!buffer) {
      debug_printf3("Encoded packet for non-existant directory: %s\n", dir);
      /* Directory does not exist, or contains 0 files */
      int len = strlen(dir) + 1;
      int neg_one = -1;
      buffer = (char *) malloc(sizeof(len) + len + sizeof(neg_one));

      memcpy(buffer + cur_pos, &len, sizeof(len));
      cur_pos += sizeof(len);
      
      memcpy(buffer + cur_pos, dir, len);
      cur_pos += len;

      memcpy(buffer + cur_pos, &neg_one, sizeof(neg_one));
      cur_pos += sizeof(neg_one);
   }
   else {
      debug_printf3("Encoded packet for directory with %d entries: %s\n", num_entries, dir);
   }

   *data = buffer;
   *len = cur_pos;

   return 0;
}

void ldcs_cache_getFirstDir(char *buffer, int size, dirbuffer_iterator_t *dpos, char **fname, char **dname)
{
   dpos->buffer = buffer;
   dpos->buffer_size = size;
   dpos->last_dirname = NULL;
   dpos->pos = 0;
   dpos->done = 0;
   if (!dpos->buffer_size) {
      *fname = NULL;
      *dname = NULL;
      return;
   }
   ldcs_cache_parseDir(dpos, fname, dname);
}

void ldcs_cache_getNextDir(dirbuffer_iterator_t *dpos, char **fname, char **dname)
{
   int length;

   if (dpos->pos == dpos->buffer_size) {
      dpos->done = 1;
      return;
   }

   assert(dpos->pos < dpos->buffer_size);
   memcpy(&length, dpos->buffer + dpos->pos, sizeof(int));
   dpos->pos += sizeof(int);
   dpos->pos += length;

   assert(dpos->pos < dpos->buffer_size);
   memcpy(&length, dpos->buffer + dpos->pos, sizeof(int));
   dpos->pos += sizeof(int);
   if (length != -1)
      dpos->pos += length;

   if (dpos->pos >= dpos->buffer_size) {
      *fname = NULL;
      *dname = NULL;
      dpos->done = 1;
      return;
   }
   ldcs_cache_parseDir(dpos, fname, dname);
}

int ldcs_cache_lastDir(dirbuffer_iterator_t *dpos)
{
   return (dpos->pos > dpos->buffer_size) || dpos->done;
}

void ldcs_cache_parseDir(dirbuffer_iterator_t *dpos, char **fname, char **dname)
{
   int length;
   int pos = dpos->pos;

   if (pos >= dpos->buffer_size) {
      dpos->done = 1;
      return;
   }

   memcpy(&length, dpos->buffer + pos, sizeof(int));
   pos += sizeof(int);
   *fname = dpos->buffer + pos;
   pos += length;

   memcpy(&length, dpos->buffer + pos, sizeof(int));
   pos += sizeof(int);
   if (length == 0 && dpos->last_dirname) {
      *dname = dpos->last_dirname;
   }
   else if (length == 0) {
      *dname = NULL;
   }
   else if (length == -1) {
      *dname = *fname;
      *fname = NULL;
   }
   else {
      *dname = dpos->buffer + pos;
      dpos->last_dirname = *dname;
      pos += length;
   }
}

void ldcs_cache_addFileDir(char *dname, char *fname)
{   
   debug_printf3("Adding directory %s, file %s to cache\n", dname, fname);
   ldcs_hash_addEntry(dname, fname);
}

int ldcs_cache_init() {
  int rc=0;
  ldcs_hash_init();
  return(rc);
}

int ldcs_cache_dump(char *filename) {
  int rc=0;
  ldcs_hash_dump(filename);
  return(rc);
}

int directoryParsed(char *dirname) {
   struct ldcs_hash_entry_t *e = ldcs_hash_Lookup(dirname);
   return e ? 1 : 0;
}

void addEmptyDirectory(char *dirname) {
   debug_printf3("Adding empty directory to cache: %s\n", dirname);
   ldcs_hash_addEntry("-", dirname);
}

void cacheLibraries(char *dirname, size_t *bytesread) {
   size_t len;
   debug_printf3("cacheLibraries for directory %s\n", dirname);
   
   DIR *d = opendir(dirname);
   struct dirent *dent = NULL, *entry;

   if (!d) {
     addEmptyDirectory(dirname);
     debug_printf3("Could not open directory %s, empty entry added\n", dirname);
     addEmptyDirectory(dirname);
     return;
   } else {
     ldcs_cache_addFileDir(dirname, dirname);
   }


   len = offsetof(struct dirent, d_name) + pathconf(dirname, _PC_NAME_MAX) + 1;
   entry = (struct dirent *) malloc(len);

   for (;;) {
      if (readdir_r(d, entry, &dent) != 0)
         break;
      if (dent == NULL)
         break;
      if (bytesread) *bytesread += sizeof(dent);
      if (dent->d_type != DT_LNK && dent->d_type != DT_REG && dent->d_type != DT_UNKNOWN && dent->d_type != DT_DIR) {
         continue;
      }
      
      ldcs_cache_addFileDir(dirname, dent->d_name);
   }

   closedir(d);

   free(entry);
}

char *ldcs_cache_result_to_str(ldcs_cache_result_t res)
{
   switch (res) {
      case LDCS_CACHE_DIR_PARSED_AND_EXISTS: return "parsed and exists";
      case LDCS_CACHE_DIR_PARSED_AND_NOT_EXISTS: return "parsed and doesn't exist";
      case LDCS_CACHE_DIR_NOT_PARSED: return "not parsed";
      case LDCS_CACHE_FILE_FOUND: return "file found";
      case LDCS_CACHE_FILE_NOT_FOUND: return "file not found";
      case LDCS_CACHE_UNKNOWN: return "uknown";
      default: return "INVALID STATE";
   }
}
