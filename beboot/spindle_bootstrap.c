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

#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "spindle_debug.h"
#include "ldcs_api.h"
#include "spindle_launch.h"
#include "client.h"
#include "client_api.h"
#include "script_loading.h"

#include "config.h"

#if !defined(LIBDIR)
#error Expected to be built with libdir defined
#endif


int ldcsid;
static int rankinfo[4]={-1,-1,-1,-1};
static int number;
static char *location, *number_s;
static char **cmdline;
static char *executable;
static char *client_lib;
static char *opts_s;

unsigned long opts;

char libstr_socket[] = LIBDIR "/libspindle_client_socket.so";
char libstr_pipe[] = LIBDIR "/libspindle_client_pipe.so";

#if defined(COMM_SOCKET)
static char *default_libstr = libstr_socket;
#elif defined(COMM_PIPES)
static char *default_libstr = libstr_pipe;
#else
#error Unknown connection type
#endif

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

   char *connection_str = client_get_connection_string(ldcsid);
   assert(connection_str);

   setenv("LD_AUDIT", client_lib, 1);
   setenv("LDCS_LOCATION", location, 1);
   setenv("LDCS_NUMBER", number_s, 1);
   setenv("LDCS_RANKINFO", rankinfo_str, 1);
   setenv("LDCS_CONNECTION", connection_str, 1);
   setenv("LDCS_OPTIONS", opts_s, 1);
}

static int parse_cmdline(int argc, char *argv[])
{
   if (argc < 4)
      return -1;

   location = argv[1];
   number_s = argv[2];
   number = atoi(number_s);
   opts_s = argv[3];
   opts = atoi(opts_s);
   cmdline = argv + 4;

   return 0;
}

static void get_executable()
{
   if (!(opts & OPT_RELOCAOUT)) {
      executable = *cmdline;
      return;
   }

   debug_printf2("Sending request for executable %s\n", *cmdline);
   send_file_query(ldcsid, *cmdline, &executable);
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
   
   result = establish_connection();
   if (result == -1) {
      err_printf("spindle_bootstrap failed to connect to daemons\n");
      return -1;
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
   execv(executable, cmdline);

   /**
    * Exec error handling
    **/
   error = errno;
   err_printf("Error execing app: %s\n", strerror(error));

   return -1;
}
