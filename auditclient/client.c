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

#define _GNU_SOURCE
#define __USE_GNU

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
#include "ldcs_api_opts.h"
#include "config.h"
#include "client.h"
#include "client_heap.h"
#include "client_api.h"

/* ERRNO_NAME currently refers to a glibc internal symbol. */
#define ERRNO_NAME "__errno_location"
typedef int *(*errno_location_t)(void);
static errno_location_t app_errno_location;

unsigned long opts;
int ldcsid = -1;

static int intercept_open;
static int intercept_exec;
static int intercept_stat;
static char debugging_name[32];

static char old_cwd[MAX_PATH_LEN+1];
static int rankinfo[4]={-1,-1,-1,-1};

/* compare the pointer top the cookie not the cookie itself, it may be changed during runtime by audit library  */
int use_ldcs = 1;
static const char *libc_name = NULL;

static char *concatStrings(const char *str1, const char *str2) 
{
   static char buffer[MAX_PATH_LEN+1];
   buffer[MAX_PATH_LEN] = '\0';
   snprintf(buffer, MAX_PATH_LEN, "%s/%s", str1, str2);
   return buffer;
}

static int find_libc_iterator(struct dl_phdr_info *lib,
                              size_t size, void *data)
{
   if (strstr(lib->dlpi_name, "libc"))
      libc_name = lib->dlpi_name;
   return 0;
}

static void find_libc_name()
{
   if (libc_name)
      return;
   dl_iterate_phdr(find_libc_iterator, NULL);
}

static void spindle_test_log_msg(char *buffer)
{
   test_printf("%s", buffer);
}

static int init_server_connection()
{
   char *location, *connection, *rankinfo_s, *opts_s;
   int number, result;

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
   opts = atoi(opts_s);

   if (!(opts & OPT_FOLLOWFORK)) {
      debug_printf("Disabling environment variables because we're not following forks\n");
      unsetenv("LD_AUDIT");
      unsetenv("LDCS_LOCATION");
      unsetenv("LDCS_NUMBER");
      unsetenv("LDCS_CONNECTION");
      unsetenv("LDCS_RANKINFO");
      unsetenv("LDCS_OPTIONS");
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
   return 0;
}

static void reset_server_connection()
{
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

static void test_log(const char *name)
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
      err_printf("Warning: Unable to set errno because app_errno_location not set\n");
      return;
   }
   *app_errno_location() = newerrno;
}

int client_init()
{
  LOGGING_INIT("Client");
  check_for_fork();
  if (!use_ldcs)
     return -1;

  init_server_connection();
  intercept_open = (opts & OPT_RELOCPY) ? 1 : 0;
  intercept_stat = (opts & OPT_RELOCPY) ? 1 : 0;
  intercept_exec = (opts & OPT_RELOCEXEC) ? 1 : 0;
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

ElfX_Addr client_call_binding(const char *symname, ElfX_Addr symvalue)
{
   if (intercept_open && strstr(symname, "open"))
      return redirect_open(symname, symvalue);
   if (intercept_exec && strstr(symname, "exec")) 
      return redirect_exec(symname, symvalue);
   if (intercept_stat && strstr(symname, "stat"))
      return redirect_stat(symname, symvalue);
   else if (run_tests && strcmp(symname, "spindle_test_log_msg") == 0)
      return (Elf64_Addr) spindle_test_log_msg;
   else if (!app_errno_location && strcmp(symname, ERRNO_NAME) == 0) {
      app_errno_location = (errno_location_t) symvalue;
      return symvalue;
   }
   else
      return symvalue;
}

char *client_library_load(const char *name)
{
   char *newname;

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

   debug_printf2("Send library request to server: %s\n", name);
   send_file_query(ldcsid, (char *) name, &newname);
   debug_printf2("Recv library from server: %s\n", newname ? newname : "NONE");      

   if(!newname) {
      newname = concatStrings(NOT_FOUND_PREFIX, name);
      debug_printf3("la_objsearch redirecting %s to %s\n", name, newname);
   }
   else {
      debug_printf("la_objsearch redirecting %s to %s\n", name, newname);
      patch_on_load_success(newname, name);
   }

   test_log(newname);   
   return newname;
}

