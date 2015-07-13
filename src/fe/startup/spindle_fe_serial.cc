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

#include "spindle_launch.h"
#include "spindle_debug.h"
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

static pid_t app_pid;
static pid_t daemon_pid;
static bool app_done = true;
static bool daemon_done = true;

static int startApp(int /*app_argc*/, char *app_argv[])
{
   app_pid = fork();
   if (app_pid == -1) {
      err_printf("Error forking application: %s\n", strerror(errno));
      return -1;
   }
   else if (app_pid != 0) {
      debug_printf2("Forked application pid %d\n", app_pid);
      app_done = false;
      return 0;
   }
   else {
      execvp(app_argv[0], app_argv);
      err_printf("Error exec'ing application %s\n", app_argv[0]);
      exit(-1);
      return -1;
   }
}
   
static int startDaemon(int /*daemon_argc*/, char *daemon_argv[], spindle_args_t *params)
{
   daemon_pid = fork();
   if (daemon_pid == -1) {
      err_printf("Error forking daemon: %s\n", strerror(errno));
      return -1;
   }
   else if (daemon_pid != 0) {
      debug_printf("Forked daemon pid %d\n", daemon_pid);
      daemon_done = false;
      return 0;
   }
   else {
      char port_s[32], num_ports_s[32], unique_id_s[32];
      snprintf(port_s, 32, "%u", params->port);
      snprintf(num_ports_s, 32, "%u", params->num_ports);
      snprintf(unique_id_s, 32, "%lu", params->unique_id);
      setenv("SPINDLE_SERIAL_PORT", port_s, 1);
      setenv("SPINDLE_SERIAL_NUMPORTS", num_ports_s, 1);
      setenv("SPINDLE_SERIAL_SHARED", unique_id_s, 1);

      execv(daemon_argv[0], daemon_argv);
      err_printf("Error exec'ing daemon %s\n", daemon_argv[0]);
      exit(-1);
      return -1;      
   }
}

enum collect_for_t {
   collect_app,
   collect_daemon
};

void collectResults(collect_for_t collect_for)
{
   while ((collect_for == collect_app && !app_done) ||
          (collect_for == collect_daemon && !daemon_done))
   {
      int status, result;
      result = wait(&status);
      if (result == -1) {
         err_printf("Error calling wait: %s\n", strerror(errno));
         return;
      }
      
      if (WIFEXITED(status) && result == app_pid) {
         debug_printf("App process %d exited with code %d\n", app_pid, WEXITSTATUS(status));
         app_done = true;
      }
      else if (WIFSIGNALED(status) && result == app_pid) {
         debug_printf("App process %d terminated with signal %d\n", app_pid, WTERMSIG(status));
         app_done = true;
      }
      else if (WIFEXITED(status) && result == daemon_pid) {
         if (WEXITSTATUS(status) == 0)
            debug_printf("Daemon process %d exited normally with exit code 0\n", daemon_pid);
         else
            err_printf("Daemon process %d exited with code %d\n", daemon_pid, WEXITSTATUS(status));
         daemon_done = true;
      }
      else if (WIFSIGNALED(status) && result == daemon_pid) {
         err_printf("Daemon process %d terminated with signal %d\n", daemon_pid, WTERMSIG(status));
         daemon_done = true;
      }
      else {
         err_printf("Unexpected return from wait.  result = %d, status = %d\n", result, status);
         return;
      }
   }
}

int startSerialFE(int app_argc, char *app_argv[],
                  int daemon_argc, char *daemon_argv[],
                  spindle_args_t *params)
{
   int result;
   const char *hostlist[2];

   result = startApp(app_argc, app_argv);
   if (result == -1) {
      return -1;
   }

   startDaemon(daemon_argc, daemon_argv, params); 
   if (result == -1) {
      return -1;
   }

   hostlist[0] = "localhost";
   hostlist[1] = NULL;
   result = spindleInitFE(hostlist, params);
   if (result == -1) {
      err_printf("Error in spindleInitFE\n");
      return -1;
   }
   
   collectResults(collect_app);
   

   result = spindleCloseFE(params);
   if (result == -1) {
      err_printf("Error in spindleCloseFE\n");
      return -1;
   }

   collectResults(collect_daemon);
   
   return 0;
}
