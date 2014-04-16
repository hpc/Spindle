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

#include <pwd.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string>

#include "config.h"
#include "spindle_debug.h"
#include "parseargs.h"
#include "spindle_launch.h"
#include "ldcs_api.h"
#include "parse_launcher.h"

using namespace std;

static void setupLogging(int argc, char **argv);
static void getAppCommandLine(int argc, char *argv[], int daemon_argc, char *daemon_argv[], spindle_args_t *args, int *mod_argc, char ***mod_argv);
static void getDaemonCommandLine(int *daemon_argc, char ***daemon_argv, spindle_args_t *args);

#if defined(HAVE_LMON)
extern int startLaunchmonFE(int app_argc, char *app_argv[],
                            int daemon_argc, char *daemon_argv[],
                            spindle_args_t *params);
#endif
extern int startSerialFE(int app_argc, char *app_argv[],
                         int daemon_argc, char *daemon_argv[],
                         spindle_args_t *params);

extern int startHostbinFE(string hostbin_exe,
                          int app_argc, char **app_argv,
                          spindle_args_t *params);

int main(int argc, char *argv[])
{
   int result = 0;

   setupLogging(argc, argv);

   spindle_args_t params;
   parseCommandLine(argc, argv, &params);

   int daemon_argc;
   char **daemon_argv;
   getDaemonCommandLine(&daemon_argc, &daemon_argv, &params);
   debug_printf2("Daemon CmdLine: ");
   for (int i = 0; i < daemon_argc; i++) {
      bare_printf2("%s ", daemon_argv[i]);
   }
   bare_printf2("\n");

   int app_argc;
   char **app_argv;
   getAppCommandLine(argc, argv, daemon_argc, daemon_argv, &params, &app_argc, &app_argv);     
   debug_printf2("Application CmdLine: ");
   for (int i = 0; i < app_argc; i++) {
      bare_printf2("%s ", app_argv[i]);
   }
   bare_printf2("\n");

   if (params.use_launcher == serial_launcher) {
      debug_printf("Starting application in serial mode\n");
      result = startSerialFE(app_argc, app_argv, daemon_argc, daemon_argv, &params);
   }
   else if (params.startup_type == startup_lmon) {
      debug_printf("Starting application with launchmon\n");
#if defined(HAVE_LMON)
      result = startLaunchmonFE(app_argc, app_argv, daemon_argc, daemon_argv, &params);
#else
      fprintf(stderr, "Spindle Error: Spindle was not built with LaunchMON support\n");
      err_printf("HAVE_LMON not defined\n");
      return -1;
#endif
   }
   else if (params.startup_type == startup_hostbin) {
      debug_printf("Starting application with hostbin\n");
      result = startHostbinFE(getHostbin(),
                              app_argc, app_argv, &params);
   }

   LOGGING_FINI;
   return result;
}

static void setupLogging(int argc, char **argv)
{
   LOGGING_INIT(const_cast<char *>("FE"));
   debug_printf("Spindle Command Line: ");
   for (int i = 0; i < argc; i++) {
      bare_printf("%s ", argv[i]);
   }
   bare_printf("\n");
}

static void getAppCommandLine(int argc, char *argv[], int daemon_argc, char *daemon_argv[], spindle_args_t *params, int *mod_argc, char ***mod_argv)
{
   int app_argc;
   char **app_argv;

   getAppArgs(&app_argc, &app_argv);

   ModifyArgv modargv(app_argc, app_argv, daemon_argc, daemon_argv, params);
   modargv.getNewArgv(*mod_argc, *mod_argv);
}

#if !defined(LIBEXECDIR)
#error Expected LIBEXECDIR to be defined
#endif
char spindle_daemon[] = LIBEXECDIR "/spindle_be";
char spindle_serial_arg[] = "--spindle_serial";
char spindle_lmon_arg[] = "--spindle_lmon";
char spindle_hostbin_arg[] = "--spindle_hostbin";

static void getDaemonCommandLine(int *daemon_argc, char ***daemon_argv, spindle_args_t *args)
{
   char **daemon_opts = (char **) malloc(11 * sizeof(char *));
   char number_s[32];
   int i = 0;

   snprintf(number_s, 32, "%d", args->number);

   //daemon_opts[i++] = "/usr/local/bin/valgrind";
   //daemon_opts[i++] = "--tool=memcheck";
   //daemon_opts[i++] = "--leak-check=full";
   daemon_opts[i++] = spindle_daemon;
   if (args->use_launcher == serial_launcher)
      daemon_opts[i++] = spindle_serial_arg;
   else if (args->startup_type == startup_lmon)
      daemon_opts[i++] = spindle_lmon_arg;
   else if (args->startup_type == startup_hostbin)
      daemon_opts[i++] = spindle_hostbin_arg;

#define STR2(X) #X
#define STR(X) STR2(X)
#define STR_CASE(X) case X: daemon_opts[i++] = const_cast<char *>(STR(X)); break
   switch (OPT_GET_SEC(args->opts)) {
      STR_CASE(OPT_SEC_MUNGE);
      STR_CASE(OPT_SEC_KEYLMON);
      STR_CASE(OPT_SEC_KEYFILE);
      STR_CASE(OPT_SEC_NULL);
   }
   daemon_opts[i++] = strdup(number_s);

   if (args->startup_type == startup_hostbin) {
      char port_str[32], ss_str[32], port_num_str[32];
      snprintf(port_str, 32, "%d", args->port);
      snprintf(port_num_str, 32, "%d", args->num_ports);
      snprintf(ss_str, 32, "%lu", args->unique_id);
      daemon_opts[i++] = strdup(port_str);
      daemon_opts[i++] = strdup(port_num_str);
      daemon_opts[i++] = strdup(ss_str);
   }
   daemon_opts[i] = NULL;

   *daemon_argc = i;
   *daemon_argv = daemon_opts;
}


