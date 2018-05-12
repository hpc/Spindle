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

#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "spindle_debug.h"
#include "ldcs_api.h"
#include "spindle_launch.h"
#include "client.h"
#include "client_api.h"
#include "exec_util.h"

#include "config.h"

#if !defined(LIBEXECDIR)
#error Expected to be built with libdir defined
#endif
#if !defined(PROGLIBDIR)
#error Expected to be built with proglib defined
#endif

char spindle_daemon[] = LIBEXECDIR "/spindle_be";
char spindle_interceptlib[] = PROGLIBDIR "/libspindleint.so";

int ldcsid;
unsigned int shm_cachesize;

static int rankinfo[4]={-1,-1,-1,-1};
static int number;
static unsigned int cachesize;
static char *location, *number_s;
static char **cmdline;
static char *executable;
static char *client_lib;
static char *opts_s;
static char **daemon_args;
static char *cachesize_s;

opt_t opts;

char libstr_socket_subaudit[] = PROGLIBDIR "/libspindle_subaudit_socket.so";
char libstr_pipe_subaudit[] = PROGLIBDIR "/libspindle_subaudit_pipe.so";
char libstr_biter_subaudit[] = PROGLIBDIR "/libspindle_subaudit_biter.so";

char libstr_socket_audit[] = PROGLIBDIR "/libspindle_audit_socket.so";
char libstr_pipe_audit[] = PROGLIBDIR "/libspindle_audit_pipe.so";
char libstr_biter_audit[] = PROGLIBDIR "/libspindle_audit_biter.so";

#if defined(COMM_SOCKET)
static char *default_audit_libstr = libstr_socket_audit;
static char *default_subaudit_libstr = libstr_socket_subaudit;
#elif defined(COMM_PIPES)
static char *default_audit_libstr = libstr_pipe_audit;
static char *default_subaudit_libstr = libstr_pipe_subaudit;
#elif defined(COMM_BITER)
static char *default_audit_libstr = libstr_biter_audit;
static char *default_subaudit_libstr = libstr_biter_subaudit;
#else
#error Unknown connection type
#endif

extern int spindle_mkdir(char *path);
extern char *parse_location(char *loc);

static int establish_connection()
{
   debug_printf2("Opening connection to server\n");
   ldcsid = client_open_connection(location, number);
   if (ldcsid == -1) 
      return -1;

   send_cwd(ldcsid);
   send_pid(ldcsid);
   send_location(ldcsid, location);
   send_rankinfo_query(ldcsid, &rankinfo[0], &rankinfo[1], &rankinfo[2], &rankinfo[3]);      

   return 0;
}

static void setup_environment()
{
   char rankinfo_str[256];
   snprintf(rankinfo_str, 256, "%d %d %d %d %d", ldcsid, rankinfo[0], rankinfo[1], rankinfo[2], rankinfo[3]);

   
   char *connection_str = NULL;
   if (opts & OPT_RELOCAOUT) 
      connection_str = client_get_connection_string(ldcsid);

   setenv("LD_AUDIT", client_lib, 1);
   setenv("LDCS_LOCATION", location, 1);
   setenv("LDCS_NUMBER", number_s, 1);
   setenv("LDCS_RANKINFO", rankinfo_str, 1);
   if (connection_str)
      setenv("LDCS_CONNECTION", connection_str, 1);
   setenv("LDCS_OPTIONS", opts_s, 1);
   setenv("LDCS_CACHESIZE", cachesize_s, 1);
   if (opts & OPT_SUBAUDIT) {
      char *preload_str = spindle_interceptlib;
      char *preload_env = getenv("LD_PRELOAD");
      char *preload;
      if (preload_env) {
         size_t len = strlen(preload_env) + strlen(preload_str) + 2;
         preload = malloc(len);
         snprintf(preload, len, "%s %s", preload_str, preload_env);
      }
      else {
         preload = preload_str;
      }
      setenv("LD_PRELOAD", preload, 1);
      
      if (preload_env) {
         free(preload);
      }
   }  
}

static int parse_cmdline(int argc, char *argv[])
{
   int i, daemon_arg_count;
   if (argc < 5)
      return -1;
   
   i = 1;
   if (strcmp(argv[1], "-daemon_args") == 0) {
      debug_printf("Parsing daemon args out of bootstrap launch line\n");
      daemon_arg_count = atoi(argv[2]);
      daemon_args = malloc(sizeof(char *) * (daemon_arg_count + 2));
      daemon_args[0] = spindle_daemon;
      for (i = 4; i < 3 + daemon_arg_count; i++)
         daemon_args[i - 3] = argv[i];
      daemon_args[i - 3] = NULL;
   }

   location = argv[i++];
   number_s = argv[i++];
   number = atoi(number_s);
   opts_s = argv[i++];
   opts = atol(opts_s);
   cachesize_s = argv[i++];
   cachesize = atoi(cachesize_s);
   cmdline = argv + i;
   assert(i < argc);

   return 0;
}

static void launch_daemon(char *location)
{
   /*grand-child fork, then execv daemon.  By grand-child forking we ensure that
     the app won't get confused by seeing an unknown process as a child. */
   pid_t child, gchild;
   int status, result;
   int fd;
   char unique_file[MAX_PATH_LEN+1];
   char buffer[32];

   result = spindle_mkdir(location);
   if (result == -1) {
      debug_printf("Exiting due to spindle_mkdir error\n");
      exit(-1);
   }
   snprintf(unique_file, MAX_PATH_LEN, "%s/spindle_daemon_pid", location);
   unique_file[MAX_PATH_LEN] = '\0';
   fd = open(unique_file, O_CREAT | O_EXCL | O_WRONLY, 0600);
   if (fd == -1) {
      debug_printf("Not starting daemon -- %s already exists\n", unique_file);
      return;
   }
   debug_printf("Client is spawning daemon\n");
      
   child = fork();
   if (child == 0) {
      gchild = fork();
      if (gchild != 0) {
         snprintf(buffer, sizeof(buffer), "%d\n", getpid());
         write(fd, buffer, strlen(buffer));
         close(fd);
         exit(0);
      }
      close(fd);
      result = setpgid(0, 0);
      if (result == -1) {
         err_printf("Failed to setpgid: %s\n", strerror(errno));
      }
      execv(spindle_daemon, daemon_args);
      fprintf(stderr, "Spindle error: Could not execv daemon %s\n", daemon_args[0]);
      exit(-1);
   }
   else if (child > 0) {
      close(fd);
      waitpid(child, &status, 0);
   }
}

static void get_executable()
{
   if (!(opts & OPT_RELOCAOUT)) {
      executable = *cmdline;
      return;
   }

   debug_printf2("Sending request for executable %s\n", *cmdline);
   exec_pathsearch(ldcsid, *cmdline, &executable);

   if (executable == NULL) {
      executable = *cmdline;
      err_printf("Failed to relocate executable %s\n", executable);
   }
   else {
      debug_printf("Relocated executable %s to %s\n", *cmdline, executable);
      chmod(executable, 0700);
   }
}

static void adjust_script()
{
   int result;
   char **new_cmdline;
   char *new_executable;

   if (!(opts & OPT_RELOCAOUT)) {
      return;
   }

   if (!executable)
      return;

   result = adjust_if_script(*cmdline, executable, cmdline, &new_executable, &new_cmdline);
   if (result != 0)
      return;

   cmdline = new_cmdline;
   executable = new_executable;
}

static void get_clientlib()
{
   char *default_libstr = (opts & OPT_SUBAUDIT) ? default_subaudit_libstr : default_audit_libstr;

   if (!(opts & OPT_RELOCAOUT)) {
      client_lib = default_libstr;
      return;
   }

   send_file_query(ldcsid, default_libstr, &client_lib);
   if (client_lib == NULL) {
      client_lib = default_libstr;
      err_printf("Failed to relocate client library %s\n", default_libstr);
   }
   else {
      debug_printf("Relocated client library %s to %s\n", default_libstr, client_lib);
      chmod(client_lib, 0600);
   }
}

int get_relocated_file(int fd, const char *name, char** newname)
{
   return send_file_query(fd, (char *) name, newname);
}

/**
 * Realize takes the 'realpath' of a non-existant location.
 * If later directories in the path don't exist, it'll cut them
 * off, take the realpath of the ones that do, then append them
 * back to the resulting realpath.
 **/
static char *realize(char *path)
{
   char *result;
   char *origpath, *cur_slash = NULL, *trailing;
   struct stat buf;
   char newpath[MAX_PATH_LEN+1];
   int lastpos;
   newpath[MAX_PATH_LEN] = '\0';

   origpath = strdup(path);
   for (;;) {
      if (stat(origpath, &buf) != -1)
         break;
      if (cur_slash)
         *cur_slash = '/';
      cur_slash = strrchr(origpath, '/');
      if (!cur_slash)
         break;
      *cur_slash = '\0';
   }
   if (cur_slash)
      trailing = cur_slash + 1;
   else
      trailing = "";

   result = realpath(origpath, newpath);
   if (!result) {
      free(origpath);
      return path;
   }

   strncat(newpath, "/", MAX_PATH_LEN);
   strncat(newpath, trailing, MAX_PATH_LEN);
   newpath[MAX_PATH_LEN] = '\0';
   free(origpath);

   lastpos = strlen(newpath)-1;
   if (lastpos >= 0 && newpath[lastpos] == '/')
      newpath[lastpos] = '\0';

   debug_printf2("Realized %s to %s\n", path, newpath);
   return strdup(newpath);
}

int main(int argc, char *argv[])
{
   int error, result;
   char **j;

   LOGGING_INIT_PREEXEC("Client");
   debug_printf("Launched Spindle Bootstrapper\n");

   result = parse_cmdline(argc, argv);
   if (result == -1) {
      fprintf(stderr, "spindle_boostrap cannot be invoked directly\n");
      return -1;
   }
   location = parse_location(location);
   if (!location) {
      return -1;
   }
   location = realize(location);

   if (daemon_args) {
      launch_daemon(location);
   }
   
   if (opts & OPT_RELOCAOUT) {
      result = establish_connection();
      if (result == -1) {
         err_printf("spindle_bootstrap failed to connect to daemons\n");
         return -1;
      }
   }

   get_executable();
   get_clientlib();
   adjust_script();
   
   /**
    * Exec setup
    **/
   debug_printf("Spindle bootstrap launching: ");
   if (!executable) {
      bare_printf("<no executable given>");
   }
   else {
      bare_printf("%s.  Args:  ", executable);
      for (j = cmdline; *j; j++) {
         bare_printf("%s ", *j);
      }
   }
   bare_printf("\n");

   /**
    * Exec the user's application.
    **/
   setup_environment();
   execvp(executable, cmdline);

   /**
    * Exec error handling
    **/
   error = errno;
   err_printf("Error execing app: %s\n", strerror(error));

   return -1;
}
