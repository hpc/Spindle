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

#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "ldcs_cobo.h"
#include "spindle_debug.h"
#include "rshlaunch.h"
#include "config.h"

static int daemon_argc;
static char **daemon_argv;
static int rsh_start_daemon(const char *hostname);
static char *rsh_cmd = NULL;
static int is_fe;
static pid_t fe_rsh_pid = (pid_t) -1;
static int daemon_argv_needs_free = 0;

#if !defined(LIBEXECDIR)
#error SPINDLE_BE_PATH not defined
#endif

#if !defined(STR)
#define XSTR(X) #X
#define STR(X) XSTR(X)
#endif

#define SPINDLE_BE_PATH LIBEXECDIR "/spindle_be"

static char *get_rsh_command()
{
   char *rsh_command;
   rsh_command = getenv("SPINDLE_RSH");
   if (rsh_command)
      return rsh_command;
   rsh_command = getenv("SPINDLE_SSH");
   if (rsh_command)
      return rsh_command;   
#if defined(RSHLAUNCH_CMD)
   rsh_command = (char *) ((RSHLAUNCH_CMD && *RSHLAUNCH_CMD) ? RSHLAUNCH_CMD : NULL);
   if (rsh_command)
      return rsh_command;
#endif
   return "rsh";
}

void init_rsh_launch_fe(spindle_args_t *args)
{
   int n = 0;
   char sec_mode_str[32], number_str[32], port_str[32], num_ports_str[32], unique_id_str[32], debug_str[32];
   char *pwd, *debug;

   if (!(args->opts & OPT_RSHLAUNCH))
      return;
   
   is_fe = 1;

   rsh_cmd = get_rsh_command();

   snprintf(sec_mode_str, sizeof(sec_mode_str), "%d", (int) OPT_GET_SEC(args->opts));
   snprintf(number_str, sizeof(number_str), "%u",  args->number);
   snprintf(port_str, sizeof(port_str), "%u", args->port);
   snprintf(num_ports_str, sizeof(num_ports_str), "%u", args->num_ports);
   snprintf(unique_id_str, sizeof(unique_id_str), "%lu", (unsigned long) args->unique_id);
   snprintf(debug_str, sizeof(debug_str), "%d", (int) spindle_debug_prints);

#define NUM_SPINDLEBE_ARGS 15
   daemon_argv = (char **) malloc(sizeof(char*) * NUM_SPINDLEBE_ARGS);
   daemon_argv[n++] = strdup(SPINDLE_BE_PATH);
   //Don't have environment forwarding.  So add SPINDLE_DEBUG, SPINDLE_TEST, and PWD to command line.
   daemon_argv[n++] = strdup("--spindle_debuglevel");
   daemon_argv[n++] = strdup(debug_str);
   pwd = getcwd(NULL, 0);
   if (pwd) {
      daemon_argv[n++] = strdup("--pwd");
      daemon_argv[n++] = pwd;
   }
   debug = getenv("SPINDLE_TEST");
   if (!debug)
      debug = "0";
   daemon_argv[n++] = strdup("--test");
   daemon_argv[n++] = strdup(debug);
   
   daemon_argv[n++] = strdup("--spindle_rshlaunch");
   daemon_argv[n++] = strdup(rsh_cmd);
   daemon_argv[n++] = strdup(sec_mode_str);
   daemon_argv[n++] = strdup(number_str);
   daemon_argv[n++] = strdup(port_str);
   daemon_argv[n++] = strdup(num_ports_str);
   daemon_argv[n++] = strdup(unique_id_str);
   daemon_argv[n] = NULL;
   daemon_argv_needs_free = 1;
   assert(n < NUM_SPINDLEBE_ARGS);
   daemon_argc = n;

   debug_printf2("Registering preconnect callback\n");
   cobo_register_preconnect_cb(rsh_start_daemon);
}

int collect_rsh_pid_fe()
{
   int result, status, error;
   
   debug_printf2("Collecting RSH pid %d\n", fe_rsh_pid);
   if (fe_rsh_pid == (pid_t) -1)
      return 0;

   for (;;) {
      result = waitpid(fe_rsh_pid, &status, 0);
      if (result == -1 && errno != EINTR) {
         error = errno;
         err_printf("Error calling waitpid(%d): %s", (int) fe_rsh_pid, strerror(error));
         return -1;
      }
      else if (WIFEXITED(status)) {
         debug_printf("RSH process exited with code %d\n", WEXITSTATUS(status));
         return (int) WEXITSTATUS(status);
      }
      else if (WIFSIGNALED(status)) {
         return -1;
      }
   }   
}

pid_t get_fe_rsh_pid()
{
   return fe_rsh_pid;
}

void clear_fe_rsh_pid()
{
   debug_printf3("Clearing fe_rsh_pid from value %d to -1\n", (int) fe_rsh_pid);
   fe_rsh_pid = (pid_t) -1;
}

void init_rsh_launch_be(int argc, char **argv)
{
   int i, j;
   int has_dochild = -1;
   char **new_argv;
   pid_t pid;
   int n = 0;

   is_fe = 0;
   
   debug_printf2("Checking whether we are doing rsh launching\n");
   for (i = 0; i < argc; i++) {
      if (strcmp(argv[i], "--spindle_rshlaunch") == 0) {
         if (i+1 == argc) {
            err_printf("ERROR: Malformed rsh commandline\n");
            exit(-1);
         }
         rsh_cmd = argv[i+1];
      }

      if (strcmp(argv[i], "--dochild") == 0) {
         has_dochild = i;
      }
   }
   if (!rsh_cmd) {
      debug_printf2("No rsh launch\n");
      return;
   }
   debug_printf2("RSH launch enabled.  %s dochild. Rsh command is %s\n",
                 (has_dochild == -1) ? "Do not have" : "Have", rsh_cmd);

   //We'll do a nohup style fork here and re-exec ourselves to
   // close the parent's rsh session.  Strip the --dochild
   // so we don't re-nohup.
   if (has_dochild != -1) {
      new_argv = (char **) malloc(sizeof(char *) * (argc+1));
      j = 0;
      for (i = 0; i < argc; i++) {
         if (i == has_dochild)
            continue;
         new_argv[j++] = argv[i];
      }
      new_argv[j] = NULL;

      pid = fork();
      if (pid) {
         exit(0);
      }
      else {
         close(0);
         close(1);
         close(2);
         open("/dev/null", O_RDONLY);
         open("/dev/null", O_WRONLY);
         open("/dev/null", O_WRONLY);
         
         execv(SPINDLE_BE_PATH, new_argv);
         err_printf("Failed to exec BE path %s\n", SPINDLE_BE_PATH);
         exit(-1);
      }
   }

   //Add the --dochild back to the command line so our children
   // will do the nohup.
   new_argv = (char **) malloc(sizeof(char*) * (argc+2));
   for (i = 0; i < argc; i++) {
      new_argv[n++] = argv[i];
   }
   new_argv[n++] = "--dochild";
   new_argv[n++] = NULL;

   daemon_argc = n-1;
   daemon_argv = new_argv;

   debug_printf2("Registering preconnect callback\n");
   cobo_register_preconnect_cb(rsh_start_daemon);   
}

static int rsh_start_daemon(const char *hostname)
{
   pid_t pid;
   int result, status;
   char **new_argv;
   int i = 0, j = 0;

   debug_printf3("Starting child daemon on %s with rsh\n", hostname);

   new_argv = (char **) malloc(sizeof(char *) * (daemon_argc+3));
   new_argv[j++] = rsh_cmd;
   new_argv[j++] = (char *) hostname;
   for (i = 0; i < daemon_argc; i++) {
      new_argv[j++] = daemon_argv[i];
   }
   new_argv[j++] = NULL;

   if (spindle_debug_prints) {
      debug_printf3("rsh daemon launch line: ");
      for (i = 0; i < j; i++) {
         bare_printf3("%s ", new_argv[i] ? : "<NULL>");
      }
      bare_printf3("\n");
   }
   
   pid = fork();
   if (pid) {
      debug_printf3("Forked process %d to be our rsh proc\n", (int) pid);

      if (!is_fe) {
         for(;;) {
            result = waitpid(pid, &status, 0);
            if (result == -1 && errno == EINTR)
               continue;
            else if (WIFSIGNALED(status)) {
               err_printf("Child rsh exited with signal %d\n", WTERMSIG(status));
               return -1;
            }
            else if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
               err_printf("Rsh did not exit with code 0.  Got %d instead\n", WEXITSTATUS(status));
               return -1;
            }
            else if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
               break;
            }
            else {
               err_printf("Unexpected return from waitpid.  result = %d, status = %d\n", result, status);
               return -1;
            }
         }
      }
      else {
         fe_rsh_pid = pid;
      }

      if (daemon_argv_needs_free) {
         char **s;
         for (s = daemon_argv; *s != NULL; s++) {
            free(*s);
         }
         free(daemon_argv);
         daemon_argv_needs_free = 0;
      }
      free(new_argv);
      
      debug_printf3("rsh to hostname %s completed successfully\n", hostname);
      return 0;
   }
   else {
      execvp(new_argv[0], new_argv);
      err_printf("Exec for rsh command %s did not succeed\n", new_argv[0]);
      exit(-1);
   }   
}
