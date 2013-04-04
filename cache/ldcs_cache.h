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
ldcs_cache_result_t ldcs_cache_findFileInCache(char *filename, char **newname, char **localpath);
ldcs_cache_result_t ldcs_cache_findFileInCachePrio(char *filename, char **newname, char **localpath);
ldcs_cache_result_t ldcs_cache_findFileDirInCache(char *filename, char *dirname, char **newname, char **localpath);
ldcs_cache_result_t ldcs_cache_findFileDirInCachePrio(char *filename, char *dirname, char **newname, char **localpath);
ldcs_cache_result_t ldcs_cache_processDirectory(char *dirname);

ldcs_hash_object_status_t ldcs_cache_addEntry(char *filename, char *dirname);
ldcs_cache_result_t ldcs_cache_updateLocalPath(char *filename, char *dirname, char *localpath);
ldcs_cache_result_t ldcs_cache_updateStatus(char *filename, char *dirname, ldcs_hash_object_status_t ostate);
ldcs_hash_object_status_t ldcs_cache_getStatus(char *filename);

int ldcs_cache_getNewEntriesSerList(char **data, int *len);
int ldcs_cache_storeNewEntriesSerList(char *data, int len);

int ldcs_cache_init();
int ldcs_cache_dump(char *filename);

/* internal */
char *findDirForFile(const char *name, const char *remote_cwd); 
char *lookupDirectoryForFile(const char *filename);
int directoryParsed(char *dirname);

void processDirectory(char *dirname);
int directoryParsedAndExists(char *dirname);

void cacheLibraries(char *dirname);

int mangleFileDirName(const char *filename, const char *dirname, char **mname);
int mangleName(char **mname);

int parseFilename(const char *name, const char *remote_cwd, char **filename, char **dirname);
int parseFilenameExact(const char *name, const char *remote_cwd, char **filename, char **dirname);

void addCWDtoPath(const char *path, const char *cwd, char **newpath);
void breakUpFilename(const char *name, char **filename, char **dirname);
char *concatStrings(const char *str1, int str1_len, const char *str2, int str2_len);
char *replacePatString( char const * const original, char const * const pattern, char const * const replacement);

#endif
