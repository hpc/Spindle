/*
This file is part of Spindle.  For copyright information see the COPYRIGHT 
file in the top level directory, or at 
https://github.com/hpc/Spindle/blob/master/COPYRIGHT

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License (as published by the Free Software
Foundation) version 2.1 dated February 1999.  This program is distributed in the
hope that it will be useful, but WITHOUT ANY WARRANTY; without even the IMPLIED
WARRANTY OF MECHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms 
and conditions of the GNU Lesser General Public License for more details.  You should 
have received a copy of the GNU Lesser General Public License along with this 
program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "spindle_debug.h"
#include "shmcache.h"
#include "config.h"
#include "client.h"
#include "client_heap.h"
#include "client_api.h"
#include "ccwarns.h"
#include "fileutil.h"

#define SPINDLE_ENODIR -68
#define SPINDLE_ENODIR_STR "NODR"

static int read_stat(char *localname, struct stat *buf)
{
   return read_buffer(localname, (char *) buf, sizeof(*buf));
}

static int read_readlink(char *localname, char *readlink_path, int *readlink_errcode, ssize_t *readlink_result)
{
   int result, fd;
   struct stat buf;
   *readlink_errcode = 0;
   *readlink_result = 0;

   fd = open(localname, O_RDONLY);
   if (fd == -1) {
      err_printf("Failed to open %s for reading: %s\n", localname, strerror(errno));
      return -1;
   }

   result = read_n_bytes(localname, fd, &buf, sizeof(buf));
   if (result == -1) {
      err_printf("Failed reading stat part of readlink: %s\n", localname);
      close(fd);
      return -1;
   }
   debug_printf3("stat (%ld)\n", sizeof(buf));

   result = read_n_bytes(localname, fd, readlink_errcode, sizeof(*readlink_errcode));
   if (result == -1) {
      err_printf("Failed reading result part of readlink: %s\n", localname);
      close(fd);
      return -1;
   }
   debug_printf3("stat errcode = %d (%ld)\n", *readlink_errcode, sizeof(*readlink_errcode));
   
   result = read_n_bytes(localname, fd, readlink_result, sizeof(*readlink_result));
   if (result == -1) {
      err_printf("Failed reading result part of readlink: %s\n", localname);
      close(fd);
      return -1;
   }
   debug_printf3("stat result = %ld (%ld)\n", *readlink_result, sizeof(*readlink_result));

   if (*readlink_result <= 0) {
      close(fd);
      return 0;
   }
   if (*readlink_result >= MAX_PATH_LEN) {
      err_printf("Invalid contents of stat file %s with %ld >= %ld\n", localname,
                 *readlink_result, (long int) MAX_PATH_LEN);
      close(fd);
      return -1;
   }

   result = read_n_bytes(localname, fd, readlink_path, *readlink_result);
   if (result == -1) {
      err_printf("Failed reading path part of readlink: %s\n", localname);
      close(fd);
      return -1;
   }
   close(fd);
   return 0;
}

int fetch_from_cache(const char *name, char **newname)
{
   int result;
   char *result_name;
   result = shmcache_lookup_or_add(name, &result_name);
   if (result == -1) {
      debug_printf2("Added placeholder for %s to shmcache and fetching from server\n",
                    name);
      return 0;
   }

   debug_printf2("Shared cache has mapping from %s (%p) to %s (%p)\n", name, name,
                 (result_name == in_progress) ? "[IN PROGRESS]" :
                 (result_name ? result_name : "[NOT PRESENT]"),
                 result_name);
   if (result_name == in_progress) {
      debug_printf("Waiting for update to %s\n", name);
      result = shmcache_waitfor_update(name, &result_name);
      if (result == -1) {
         debug_printf("Entry for %s deleted while waiting for update\n", name);
         return 0;
      }
   }
   
   *newname = result_name ? spindle_strdup(result_name) : NULL;
   return 1;
}

GCC7_DISABLE_WARNING("-Wformat-truncation")
static void get_cache_name(const char *path, const char *prefix, char *result, char *dirresult)
{
   char cwd[MAX_PATH_LEN+1];
   char *last_slash = NULL;

   if (path[0] != '/') {
      (void)! getcwd(cwd, MAX_PATH_LEN+1);
      cwd[MAX_PATH_LEN] = '\0';
      snprintf(result, MAX_PATH_LEN+strlen(prefix), "%s%s/%s", prefix, cwd, path);
      snprintf(dirresult, MAX_PATH_LEN+1, "^%s/%s", cwd, path);
   }
   else {
      snprintf(result, MAX_PATH_LEN+strlen(prefix), "%s%s", prefix, path);
      snprintf(dirresult, MAX_PATH_LEN+1, "^%s", path);
   }
   last_slash = strrchr(dirresult, '/');
   if (last_slash)
      *last_slash = '\0';
}
GCC7_ENABLE_WARNING

static int check_cache(const char *path, const char *prefix, char *cache_name, char *dir_name,
                       int nodir_errcode, int *errcode, char **result_name)
{
   char *dirresult;
   int result;

   get_cache_name(path, prefix, cache_name, dir_name);

   debug_printf3("Lookup directory %s for existing in shmcache before doing full lookup.\n", dir_name);
   result = shmcache_lookup(dir_name, &dirresult);
   if (result != -1 && strcmp(dirresult, SPINDLE_ENODIR_STR) == 0) {
      debug_printf2("Shm cache reports directory not present for file %s.  Short-circuiting to errcode %d\n",
                    path, nodir_errcode);
      *errcode = nodir_errcode;
      *result_name = NULL;
      return 1;
   }
   
   result = fetch_from_cache(cache_name, result_name);
   if (!result)
      return 0;

   debug_printf3("File %s exist in cache as %s\n", cache_name, *result_name);
   if (strncmp(*result_name, "ERRNO:", 6) == 0) {
      *errcode = atoi((*result_name)+6);
      if (*errcode == SPINDLE_ENODIR)
         *errcode = nodir_errcode;
      
      spindle_free(*result_name);
      *result_name = NULL;
   }
   else {
      *errcode = 0;
   }
   return 1;
}

static int update_cache(char *cache_name, char *dir_name, char *result_name, int *errcode, int nodir_errcode)
{
   char errstr[64];

   if (result_name && !*errcode) {
      debug_printf3("Updating cache value %s to %s\n", cache_name, result_name);
      shmcache_update(cache_name, result_name);
      return 0;
   }

   if (*errcode == SPINDLE_ENODIR) {
      debug_printf3("Setting directory %s to not exist in shmcache\n", dir_name);
      shmcache_add(dir_name, SPINDLE_ENODIR_STR);
      *errcode = nodir_errcode;
   }

   snprintf(errstr, sizeof(errstr), "ERRNO:%d", *errcode);
   shmcache_update(cache_name, errstr);
   return 0;
}

int get_existance_test(int fd, const char *path, int *exists)
{
   int use_cache = (opts & OPT_SHMCACHE);
   int found_file, errcode, result = 0;
   char cache_name[MAX_PATH_LEN+2], dir_name[MAX_PATH_LEN+2];
   char *exist_str = NULL;

   if (use_cache) {
      debug_printf2("Looking up file existance for %s in shared cache\n", path);
      found_file = check_cache(path, "&", cache_name, dir_name, ENOENT, &errcode, &exist_str);
      if (found_file) {
         *exists = (exist_str && *exist_str == 'y');
         spindle_free(exist_str);
         return 0;
      }
   }

   debug_printf3("Sending existance test for %s to server\n", path);
   result = send_existance_test(fd, (char *) path, exists);
   debug_printf3("Existance test for %s returned exists: %d, result: %d\n",
                 path, *exists, result);

   if (use_cache) {
      exist_str = *exists ? "y" : "n";
      update_cache(cache_name, dir_name, exist_str, &errcode, ENOENT);
      spindle_free(exist_str);
   }

   return result;
}

static int get_metadata_result(int fd, const char *path, int is_lstat, int *exists, struct stat *buf, char *readlink_path, ssize_t *readlink_result, int *readlink_errcode)
{
   int result, errcode = 0, network_result = 0;
   char cache_name[MAX_PATH_LEN+3], dir_name[MAX_PATH_LEN+2];
   char buffer[MAX_PATH_LEN+1];
   char *newpath = NULL;
   int use_cache = (opts & OPT_SHMCACHE);
   int found_file = 0;
   buffer[0] = '\0';

   if (use_cache) {
      debug_printf2("Looking up %s stat for %s in shared cache\n", is_lstat ? "l" : "", path);
      found_file = check_cache(path, is_lstat ? "**" : "*", cache_name, dir_name, 
                               ENOENT, &errcode, &newpath);
      if (found_file) {
         debug_printf3("Found stat for %s in cache\n", path);
         strncpy(buffer, newpath, (sizeof(buffer)-1));
         buffer[sizeof(buffer)-1] = '\0';
         spindle_free(newpath);
         newpath = NULL;
      }
   }

   if (!found_file) {
      debug_printf2("Sending request for %sstat of %s to server\n", is_lstat ? "l" : "", path);
      network_result = send_stat_request(fd, (char *) path, is_lstat, buffer);
      if (network_result == -1)
         buffer[0] = '\0';
      if (network_result == STAT_SELF) {
         debug_printf3("Returning STAT_SELF_OPEN from get_metadata for %s\n", path);
         return STAT_SELF_OPEN;
      }
      if (network_result == DISCONNECT) {
         debug_printf3("Disconnected. Returning original stat for %s\n", path);
         return STAT_SELF_OPEN;
      }
      debug_printf2("Server returned stat result for %s: %s\n", path, buffer);

      if (use_cache)
         update_cache(cache_name, dir_name, buffer, &errcode, ENOENT);
   }

   if (buffer[0] == '\0') {
      debug_printf3("Returning %d with not exists for metadata read\n", network_result);
      *exists = 0;
      return network_result;
   }
   *exists = 1;

   test_log(buffer);

   if (buf)
      result = read_stat(buffer, buf);
   else if (readlink_path && readlink_result)
      result = read_readlink(buffer, readlink_path, readlink_errcode, readlink_result);
   else
      assert(0);
   
   if (result == -1) {
      err_printf("Failed to read stat info for %s from %s\n", path, newpath);
      *exists = 0;
      return -1;
   }
   return network_result;
}

int get_stat_result(int fd, const char *path, int is_lstat, int *exists, struct stat *buf)
{
   return get_metadata_result(fd, path, is_lstat, exists, buf, NULL, NULL, NULL);
}

int get_readlink_result(int fd, const char *path, char *readlink_path, ssize_t *readlink_result, int *readlink_errcode)
{
   int exists, result;
   result = get_metadata_result(fd, path, 1, &exists, NULL, readlink_path, readlink_result, readlink_errcode);
   if (result == -1)
      return -1;
   if (result == STAT_SELF_OPEN)
      return STAT_SELF_OPEN;
   if (!exists) {
      *readlink_result = -1;
      *readlink_errcode = ENOENT;
      return 0;
   }
   return result;
}

int get_relocated_file(int fd, const char *name, int dso, char** newname, int *errorcode, int *direxists)
{
   int use_cache = (opts & OPT_SHMCACHE);
   int found_file = 0, result;
   char cache_name[MAX_PATH_LEN+2], dir_name[MAX_PATH_LEN+2];

   if (use_cache) {
      debug_printf2("Looking up %s in shared cache\n", name);
      found_file = check_cache(name, "", cache_name, dir_name, SPINDLE_ENODIR, errorcode, newname);
      if (found_file) {
         if (direxists) 
            *direxists = (*errorcode == SPINDLE_ENODIR) ? 0 : 1;
         if (*errorcode == SPINDLE_ENODIR)
            *errorcode = ENOENT;
         return 0;
      }
   }

   debug_printf2("Send file request to server: %s\n", name);
   result = send_file_query(fd, (char *) name, dso, newname, errorcode);
   if (result == DISCONNECT) {
      return DISCONNECT;
   }
   debug_printf2("Recv file from server: %s\n", *newname ? *newname : "NONE");

   if (use_cache) {
      update_cache(cache_name, dir_name, *newname, errorcode, ENOENT);
   }
   if (direxists) 
      *direxists = (*errorcode == SPINDLE_ENODIR) ? 0 : 1;
   if (*errorcode == SPINDLE_ENODIR)
      *errorcode = ENOENT;
   

   return result;
}

