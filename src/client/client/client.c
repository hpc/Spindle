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

#define _GNU_SOURCE

#include <dirent.h>
#include <unistd.h>
#include <stddef.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <link.h>
#include <stdlib.h>
#include <stdio.h>

#include "ldcs_api.h" 
#include "config.h"
#include "client.h"
#include "client_heap.h"
#include "client_api.h"
#include "spindle_launch.h"
#include "shmcache.h"

errno_location_t app_errno_location;

opt_t opts;
int ldcsid = -1;
unsigned int shm_cachesize;
static unsigned int shm_cache_limit;

int intercept_open;
int intercept_exec;
int intercept_stat;
int intercept_close;
int intercept_fork;
static char debugging_name[32];

static char old_cwd[MAX_PATH_LEN+1];
static int rankinfo[4]={-1,-1,-1,-1};

extern char *parse_location(char *loc);

/* compare the pointer top the cookie not the cookie itself, it may be changed during runtime by audit library  */
int use_ldcs = 1;
static const char *libc_name = NULL;
static const char *interp_name = NULL;
static const ElfW(Phdr) *libc_phdrs, *interp_phdrs;
static int num_libc_phdrs, num_interp_phdrs;
ElfW(Addr) libc_loadoffset, interp_loadoffset;
char *location;
int number;

static char *concatStrings(const char *str1, const char *str2) 
{
   static char buffer[MAX_PATH_LEN+1];
   buffer[MAX_PATH_LEN] = '\0';
   snprintf(buffer, MAX_PATH_LEN, "%s/%s", str1, str2);
   return buffer;
}

static int find_libs_iterator(struct dl_phdr_info *lib,
                              size_t size, void *data)
{
   if (!libc_name && (strstr(lib->dlpi_name, "libc.") || strstr(lib->dlpi_name, "libc-"))) {
      libc_name = lib->dlpi_name;
      libc_phdrs = lib->dlpi_phdr;
      libc_loadoffset = lib->dlpi_addr;
      num_libc_phdrs = (int) lib->dlpi_phnum;
   }
   else if (!interp_name) {
      const ElfW(Phdr) *phdrs = lib->dlpi_phdr;
      unsigned long r_brk = _r_debug.r_brk;
      unsigned int phdrs_size = lib->dlpi_phnum, i;

      if (!phdrs) {
         /* ld.so bug?  Seeing NULL PHDRS for dynamic linker entry. */
         interp_name = lib->dlpi_name;
      } 
      else {
         for (i = 0; i < phdrs_size; i++) {
            if (phdrs[i].p_type == PT_LOAD) {
               unsigned long base = phdrs[i].p_vaddr + lib->dlpi_addr;
               if (base <= r_brk && r_brk < base + phdrs[i].p_memsz) {
                  interp_name = lib->dlpi_name;
                  break;
               }
            }
         }
      }
      if (interp_name) {
         num_interp_phdrs = phdrs_size;
         interp_phdrs = phdrs;
         interp_loadoffset = lib->dlpi_addr;
      }
   }

   return 0;
}

char *find_libc_name()
{
   if (libc_name)
      return (char *) libc_name;
   dl_iterate_phdr(find_libs_iterator, NULL);
   return (char *) libc_name;
}

const ElfW(Phdr) *find_libc_phdrs(int *num_phdrs)
{
   if (libc_phdrs) {
      *num_phdrs = num_libc_phdrs;
      return libc_phdrs;
   }
   dl_iterate_phdr(find_libs_iterator, NULL);
   *num_phdrs = num_libc_phdrs;
   return libc_phdrs;
}

ElfW(Addr) find_libc_loadoffset()
{
   if (libc_phdrs)
      return libc_loadoffset;
   dl_iterate_phdr(find_libs_iterator, NULL);
   return libc_loadoffset;
}

char *find_interp_name()
{
   if (interp_name)
      return (char *) interp_name;
   dl_iterate_phdr(find_libs_iterator, NULL);
   return (char *) interp_name;
}

const ElfW(Phdr) *find_interp_phdrs(int *num_phdrs)
{
   if (interp_name) {
      *num_phdrs = num_interp_phdrs;
      return interp_phdrs;
   }
   dl_iterate_phdr(find_libs_iterator, NULL);
   *num_phdrs = num_interp_phdrs;
   return interp_phdrs;
}

ElfW(Addr) find_interp_loadoffset()
{
   if (interp_name)
      return interp_loadoffset;
   dl_iterate_phdr(find_libs_iterator, NULL);
   return interp_loadoffset;
}

void int_spindle_test_log_msg(char *buffer)
{
   test_printf("%s", buffer);
}

static int init_server_connection()
{
   char *connection, *rankinfo_s, *opts_s, *cachesize_s;

   debug_printf("Initializing connection to server\n");

   if (ldcsid != -1)
      return 0;
   if (!use_ldcs)
      return 0;

   location = getenv("LDCS_LOCATION");
   number = atoi(getenv("LDCS_NUMBER"));
   connection = getenv("LDCS_CONNECTION");
   rankinfo_s = getenv("LDCS_RANKINFO");
   opts_s = getenv("LDCS_OPTIONS");
   cachesize_s = getenv("LDCS_CACHESIZE");
   opts = atol(opts_s);
   shm_cachesize = atoi(cachesize_s) * 1024;

   if (strchr(location, '$')) {
      location = parse_location(location);
   }

   if (!(opts & OPT_FOLLOWFORK)) {
      debug_printf("Disabling environment variables because we're not following forks\n");
      unsetenv("LD_AUDIT");
      unsetenv("LDCS_LOCATION");
      unsetenv("LDCS_NUMBER");
      unsetenv("LDCS_CONNECTION");
      unsetenv("LDCS_RANKINFO");
      unsetenv("LDCS_OPTIONS");
   }

   if (opts & OPT_SHMCACHE) {
      assert(shm_cachesize);
#if defined(COMM_BITER)
      shm_cache_limit = shm_cachesize > 512*1024 ? shm_cachesize - 512*1024 : 0;
#else
      shm_cache_limit = shm_cachesize;
#endif
      shmcache_init(location, number, shm_cachesize, shm_cache_limit);
   }

   if (connection) {
      /* boostrapper established the connection for us.  Reuse it. */
      debug_printf("Recreating existing connection to server\n");
      debug_printf3("location = %s, number = %d, connection = %s, rankinfo = %s\n",
                    location, number, connection, rankinfo_s);
      ldcsid  = client_register_connection(connection);
      if (ldcsid == -1)
         return -1;
      assert(rankinfo_s);
      sscanf(rankinfo_s, "%d %d %d %d", rankinfo+0, rankinfo+1, rankinfo+2, rankinfo+3);
      unsetenv("LDCS_CONNECTION");
   }
   else {
      /* Establish a new connection */
      debug_printf("open connection to ldcs %s %d\n", location, number);
      ldcsid = client_open_connection(location, number);
      if (ldcsid == -1)
         return -1;

      send_pid(ldcsid);
      send_location(ldcsid, location);
      send_rankinfo_query(ldcsid, rankinfo+0, rankinfo+1, rankinfo+2, rankinfo+3);
   }
   
   snprintf(debugging_name, 32, "Client.%d", rankinfo[0]);
   LOGGING_INIT(debugging_name);

   sync_cwd();

   if (opts & OPT_RELOCPY)
      parse_python_prefixes(ldcsid);
   return 0;
}

static void reset_server_connection()
{
   client_close_connection(ldcsid);

   ldcsid = -1;
   old_cwd[0] = '\0';

   init_server_connection();
}

void check_for_fork()
{
   static int cached_pid = 0;
   int current_pid = getpid();
   if (!cached_pid) {
      cached_pid = current_pid;
      return;
   }
   if (cached_pid == current_pid) {
      return;
   }

   if (!(opts & OPT_FOLLOWFORK)) {
      debug_printf("Client %d forked and is now process %d.  Not following fork.\n", cached_pid, current_pid);
      use_ldcs = 0;
      return;
   }
   debug_printf("Client %d forked and is now process %d.  Following.\n", cached_pid, current_pid);
   cached_pid = current_pid;
   reset_spindle_debugging();
   reset_server_connection();
}

void test_log(const char *name)
{
   int result;
   if (!run_tests)
      return;
   result = open(name, O_RDONLY);
   if (result != -1)
      close(result);
   test_printf("open(\"%s\", O_RDONLY) = %d\n", name, result);
}

void sync_cwd()
{
   char cwd[MAX_PATH_LEN+1];
   char *result;
   
   result = getcwd(cwd, MAX_PATH_LEN+1);
   if (!result) {
      err_printf("Failure to get CWD: %s\n", strerror(errno));
      return;
   }

   if (strcmp(cwd, old_cwd) == 0) {
      /* Diretory hasn't changed since last check. No actions needed. */
      return;
   }

   debug_printf("Client changed directory to %s\n", cwd);
   strncpy(old_cwd, cwd, MAX_PATH_LEN);
   old_cwd[MAX_PATH_LEN] = '\0';

   send_dir_cwd(ldcsid, old_cwd);
}

void set_errno(int newerrno)
{
   if (!app_errno_location) {
      debug_printf("Warning: Unable to set errno because app_errno_location not set\n");
      return;
   }
   *app_errno_location() = newerrno;
}

int client_init()
{
  int initial_run = 0;
  LOGGING_INIT("Client");
  check_for_fork();
  if (!use_ldcs)
     return -1;

  init_server_connection();
  intercept_open = (opts & OPT_RELOCPY) ? 1 : 0;
  intercept_stat = (opts & OPT_RELOCPY || !(opts & OPT_NOHIDE)) ? 1 : 0;
  intercept_exec = (opts & OPT_RELOCEXEC) ? 1 : 0;
  intercept_fork = 1;
  intercept_close = 1;  

  if (getenv("LDCS_BOOTSTRAPPED")) {
     initial_run = 1;
     unsetenv("LDCS_BOOTSTRAPPED");
  }
  
  if ((opts & OPT_REMAPEXEC) &&
      ((initial_run && (opts & OPT_RELOCAOUT)) ||
       (!initial_run && (opts & OPT_RELOCEXEC))))
  {
     remap_executable(ldcsid);
  }

  return 0;
}

int client_done()
{
   check_for_fork();
   if (ldcsid == -1 || !use_ldcs)
      return 0;

   debug_printf2("Done. Closing connection %d\n", ldcsid);
   send_end(ldcsid);
   client_close_connection(ldcsid);
   return 0;
}

extern int read_buffer(char *localname, char *buffer, int size);

static int read_stat(char *localname, struct stat *buf)
{
   return read_buffer(localname, (char *) buf, sizeof(*buf));
}

static int read_ldso_metadata(char *localname, ldso_info_t *ldsoinfo)
{
   return read_buffer(localname, (char *) ldsoinfo, sizeof(*ldsoinfo));
}

static int fetch_from_cache(const char *name, char **newname)
{
   int result;
   char *result_name;
   result = shmcache_lookup_or_add(name, &result_name);
   if (result == -1)
      return 0;

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

static void get_cache_name(const char *path, char *prefix, char *result)
{
   char cwd[MAX_PATH_LEN+1];

   if (path[0] != '/') {
      getcwd(cwd, MAX_PATH_LEN+1);
      cwd[MAX_PATH_LEN] = '\0';
      snprintf(result, MAX_PATH_LEN+strlen(prefix), "%s%s/%s", prefix, cwd, path);
   }
   else {
      snprintf(result, MAX_PATH_LEN+strlen(prefix), "%s%s", prefix, path);
   }
}

int get_existance_test(int fd, const char *path, int *exists)
{
   int use_cache = (opts & OPT_SHMCACHE) && (shm_cachesize > 0);
   int found_file, result;
   char cache_name[MAX_PATH_LEN+2];
   char *exist_str;

   if (use_cache) {
      debug_printf2("Looking up file existance for %s in shared cache\n", path);
      get_cache_name(path, "&", cache_name);
      cache_name[sizeof(cache_name)-1] = '\0';
      found_file = fetch_from_cache(cache_name, &exist_str);
      if (found_file) {
         *exists = (exist_str[0] == 'y');
         return 0;
      }
   }

   result = send_existance_test(fd, (char *) path, exists);
   if (result == -1)
      return -1;

   if (use_cache) {
      exist_str = *exists ? "y" : "n";
      shmcache_update(cache_name, exist_str);
   }
   return 0;
}

int get_stat_result(int fd, const char *path, int is_lstat, int *exists, struct stat *buf)
{
   int result;
   char buffer[MAX_PATH_LEN+1];
   char cache_name[MAX_PATH_LEN+3];
   char *newpath;
   int use_cache = (opts & OPT_SHMCACHE) && (shm_cachesize > 0);
   int found_file = 0;

   if (use_cache) {
      debug_printf2("Looking up %sstat for %s in shared cache\n", is_lstat ? "l" : "", path);
      get_cache_name(path, is_lstat ? "**" : "*", cache_name);
      cache_name[sizeof(cache_name)-1] = '\0';
      found_file = fetch_from_cache(cache_name, &newpath);
   }

   if (!found_file) {
      result = send_stat_request(fd, (char *) path, is_lstat, buffer);
      if (result == -1) {
         *exists = 0;
         return -1;
      }
      newpath = buffer[0] != '\0' ? buffer : NULL;

      if (use_cache) 
         shmcache_update(cache_name, newpath);
   }
   
   if (newpath == NULL) {
      *exists = 0;
      return 0;
   }
   *exists = 1;

   test_log(newpath);
   result = read_stat(newpath, buf);
   if (result == -1) {
      err_printf("Failed to read stat info for %s from %s\n", path, newpath);
      *exists = 0;
      return -1;
   }
   return 0;
}

int get_relocated_file(int fd, const char *name, char** newname, int *errorcode)
{
   int found_file = 0;
   int use_cache = (opts & OPT_SHMCACHE) && (shm_cachesize > 0);
   char cache_name[MAX_PATH_LEN+1];

   if (use_cache) {
      debug_printf2("Looking up %s in shared cache\n", name);
      get_cache_name(name, "", cache_name);
      cache_name[sizeof(cache_name)-1] = '\0';
      found_file = fetch_from_cache(cache_name, newname);
   }

   if (!found_file) {
      debug_printf2("Send file request to server: %s\n", name);
      send_file_query(fd, (char *) name, newname, errorcode);
      debug_printf2("Recv file from server: %s\n", *newname ? *newname : "NONE");      
      if (use_cache)
         shmcache_update(cache_name, *newname);
   }

   return 0;
}

char *client_library_load(const char *name)
{
   char *newname;
   int errcode;

   check_for_fork();
   if (!use_ldcs || ldcsid == -1) {
      return (char *) name;
   }
   if (!(opts & OPT_RELOCSO)) {
      return (char *) name;
   }
   
   /* Don't relocate a new copy of libc, it's always already loaded into the process. */
   find_libc_name();
   if (libc_name && strcmp(name, libc_name) == 0) {
      debug_printf("la_objsearch not redirecting libc %s\n", name);
      test_log(name);
      return (char *) name;
   }
   
   sync_cwd();

   get_relocated_file(ldcsid, name, &newname, &errcode);
 
   if(!newname) {
      newname = concatStrings(NOT_FOUND_PREFIX, name);
   }
   else {
      patch_on_load_success(newname, name);
   }

   debug_printf("la_objsearch redirecting %s to %s\n", name, newname);
   test_log(newname);
   return newname;
}

python_path_t *pythonprefixes = NULL;
void parse_python_prefixes(int fd)
{
   char *path;
   int i, j;
   int num_pythonprefixes;

   if (pythonprefixes)
      return;
   get_python_prefix(fd, &path);

   num_pythonprefixes = (path[0] == '\0') ? 0 : 1;
   for (i = 0; path[i] != '\0'; i++) {
      if (path[i] == ':')
         num_pythonprefixes++;
   }   

   debug_printf3("num_pythonprefixes = %d in %s\n", num_pythonprefixes, path);
   pythonprefixes = (python_path_t *) spindle_malloc(sizeof(python_path_t) * (num_pythonprefixes+1));
   for (i = 0, j = 0; j < num_pythonprefixes; j++) {
      char *cur = path+i;
      char *next = strchr(cur, ':');
      if (next != NULL)
         *next = '\0';
      pythonprefixes[j].path = cur;
      pythonprefixes[j].pathsize = strlen(cur);
      i += pythonprefixes[j].pathsize+1;
   }
   pythonprefixes[num_pythonprefixes].path = NULL;
   pythonprefixes[num_pythonprefixes].pathsize = 0;

   for (i = 0; pythonprefixes[i].path != NULL; i++)
      debug_printf3("Python path # %d = %s\n", i, pythonprefixes[i].path);
}

int get_ldso_metadata(signed int *binding_offset)
{
   ldso_info_t info;
   int result;
   char filename[MAX_PATH_LEN+1];

   find_interp_name();
   debug_printf2("Requesting interpreter metadata for %s\n", interp_name);
   result = send_ldso_info_request(ldcsid, interp_name, filename);
   if (result == -1)
      return -1;

   read_ldso_metadata(filename, &info);

   *binding_offset = info.binding_offset;
   return 0;
}
