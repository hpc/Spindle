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

#ifndef LDCS_CACHE_H
#define LDCS_CACHE_H

typedef enum {
  LDCS_CACHE_DIR_PARSED_AND_EXISTS,
  LDCS_CACHE_DIR_PARSED_AND_NOT_EXISTS,
  LDCS_CACHE_DIR_NOT_PARSED,
  LDCS_CACHE_FILE_FOUND,
  LDCS_CACHE_FILE_NOT_FOUND,
  LDCS_CACHE_UNKNOWN
} ldcs_cache_result_t;

typedef enum {
  LDCS_CACHE_OBJECT_STATUS_NOT_SET,
  LDCS_CACHE_OBJECT_STATUS_LOCAL_PATH,
  LDCS_CACHE_OBJECT_STATUS_GLOBAL_PATH,
  LDCS_CACHE_OBJECT_STATUS_UNKNOWN
} ldcs_hash_object_status_t;

ldcs_cache_result_t ldcs_cache_findDirInCache(char *dirname);
ldcs_cache_result_t ldcs_cache_findFileDirInCache(char *filename, char *dirname, char **localpath, int *errcode);
ldcs_cache_result_t ldcs_cache_getAlias(char *filename, char *dirname, char **alias);
ldcs_cache_result_t ldcs_cache_isReplicated(char *filename, char *dirname, int *replication);

ldcs_cache_result_t ldcs_cache_processDirectory(char *dirname, size_t *bytesread);

ldcs_cache_result_t ldcs_cache_updateEntry(char *filename, char *dirname, 
                                           char *localname, void *buffer, size_t buffer_size, char *alias_to, int is_replicated, int errcode);

ldcs_cache_result_t ldcs_cache_updateStatus(char *filename, char *dirname, ldcs_hash_object_status_t ostate);
ldcs_hash_object_status_t ldcs_cache_getStatus(char *filename);

int ldcs_cache_getNewEntriesForDir(char *dir, char **data, int *len);

int ldcs_cache_init();
int ldcs_cache_dump(char *filename);

int ldcs_cache_get_buffer(char *dirname, char *filename, void **buffer, size_t *size, char **alias_to);

char *ldcs_cache_result_to_str(ldcs_cache_result_t res);
/* Parse directory content packets */
typedef struct {
   char *buffer;
   int buffer_size;
   char *last_dirname;
   int pos;
   int done;
} dirbuffer_iterator_t;
void ldcs_cache_getFirstDir(char *buffer, int size, dirbuffer_iterator_t *dpos, char **fname, char **dname);
void ldcs_cache_getNextDir(dirbuffer_iterator_t *dpos, char **fname, char **dname);
int ldcs_cache_lastDir(dirbuffer_iterator_t *dpos);
void ldcs_cache_parseDir(dirbuffer_iterator_t *dpos, char **fname, char **dname);
void ldcs_cache_addFileDir(char *dname, char *fname);
void addEmptyDirectory(char *dirname);
#define foreach_filedir(BUFFER, SIZE, POS, FNAME, DNAME)                \
   for (ldcs_cache_getFirstDir(BUFFER, SIZE, &POS, &FNAME, &DNAME);     \
        !ldcs_cache_lastDir(&POS);                                      \
        ldcs_cache_getNextDir(&POS, &FNAME, &DNAME))



/* internal */
int directoryParsed(char *dirname);

void cacheLibraries(char *dirname, size_t *bytesread);

char *concatStrings(const char *str1, int str1_len, const char *str2, int str2_len);
#endif
