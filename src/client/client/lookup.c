#include <errno.h>
#include "spindle_debug.h"
#include "shmcache.h"
#include "config.h"
#include "client.h"
#include "client_heap.h"
#include "client_api.h"
#include "ccwarns.h"

#define SPINDLE_ENODIR -68
#define SPINDLE_ENODIR_STR "NODR"

static int read_stat(char *localname, struct stat *buf)
{
   return read_buffer(localname, (char *) buf, sizeof(*buf));
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

int get_stat_result(int fd, const char *path, int is_lstat, int *exists, struct stat *buf)
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
      debug_printf2("Server returned stat result for %s: %s\n", path, buffer);
      
      if (network_result == -1)
         buffer[0] = '\0';

      if (use_cache)
         update_cache(cache_name, dir_name, buffer, &errcode, ENOENT);
   }

   if (buffer[0] == '\0') {
      *exists = 0;
      return network_result;
   }
   *exists = 1;

   test_log(buffer);
   result = read_stat(buffer, buf);
   if (result == -1) {
      err_printf("Failed to read stat info for %s from %s\n", path, newpath);
      *exists = 0;
      return -1;
   }
   return network_result;
}

int get_relocated_file(int fd, const char *name, char** newname, int *errorcode)
{
   int use_cache = (opts & OPT_SHMCACHE);
   int found_file = 0, result;
   char cache_name[MAX_PATH_LEN+2], dir_name[MAX_PATH_LEN+2];

   if (use_cache) {
      debug_printf2("Looking up %s in shared cache\n", name);
      found_file = check_cache(name, "", cache_name, dir_name, ENOENT, errorcode, newname);
      if (found_file)
         return 0;
   }

   debug_printf2("Send file request to server: %s\n", name);
   result = send_file_query(fd, (char *) name, newname, errorcode);
   debug_printf2("Recv file from server: %s\n", *newname ? *newname : "NONE");

   if (use_cache) {
      update_cache(cache_name, dir_name, *newname, errorcode, ENOENT);
   }
   if (*errorcode == SPINDLE_ENODIR)
      *errorcode = ENOENT;

   return result;
}

