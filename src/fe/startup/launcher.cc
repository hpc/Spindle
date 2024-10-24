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

#include "launcher.h"
#include "spindle_debug.h"
#include "config.h"
#include "spindle_launch.h"
#include "parse_launcher.h"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>

#include <vector>
using namespace std;

static void mark_closeonexec(int fd)
{
   int flags = 0;

   flags = fcntl(fd, F_GETFD);
   fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
}

Launcher::Launcher(spindle_args_t *params_) :
   params(params_)
{
   int fds[2];

   (void)! pipe(fds);
   jobfinish_read_fd = fds[0];
   jobfinish_write_fd = fds[1];
   mark_closeonexec(jobfinish_read_fd);
   mark_closeonexec(jobfinish_write_fd);
   fflush(stdout);
}

Launcher::~Launcher()
{   
}

int Launcher::getJobFinishFD()
{
   return jobfinish_read_fd;
}

void Launcher::markFinished()
{
   char byte = 'b';
   int result;
   do {
      result = write(jobfinish_write_fd, &byte, sizeof(byte));
   } while (result == -1 && errno == EINTR);
   if (result == -1) {
      int error = errno;
      err_printf("Failed to mark job finished: %s\n", strerror(error));
   }
}

void Launcher::getSecondaryDaemonArgs(vector<const char *> &)
{
}

bool Launcher::setupJob(app_id_t id, int &app_argc, char** &app_argv)
{
   int mod_argc;
   char **mod_argv;

   ModifyArgv modargv(app_argc, app_argv, daemon_argc, daemon_argv, params);
   if (!modargv.getNewArgv(mod_argc, mod_argv))
      return false;

   app_argc = mod_argc;
   app_argv = mod_argv;
   
   return true;
}

#if !defined(LIBEXECDIR)
#error Expected LIBEXECDIR to be defined
#endif
static char spindle_daemon[] = LIBEXECDIR "/spindle_be";
char spindle_hostbin_arg[] = "--spindle_hostbin";

#define DAEMON_OPTS_SIZE 32
bool Launcher::setupDaemons()
{
   int i = 0;
   daemon_argv = (char **) malloc(DAEMON_OPTS_SIZE * sizeof(char *));

   //daemon_argv[i++] = "/usr/local/bin/valgrind";
   //daemon_argv[i++] = "--tool=memcheck";
   //daemon_argv[i++] = "--leak-check=full";
   daemon_argv[i++] = spindle_daemon;
   daemon_argv[i++] = const_cast<char *>(getDaemonArg());

   char security_s[32];
   snprintf(security_s, 32, "%u", (unsigned int) OPT_GET_SEC(params->opts));
   daemon_argv[i++] = strdup(security_s);

   char number_s[32];
   snprintf(number_s, 32, "%d", params->number);
   daemon_argv[i++] = strdup(number_s);

   vector<const char *> secondary_args;
   getSecondaryDaemonArgs(secondary_args);
   for (vector<const char *>::iterator j = secondary_args.begin(); j != secondary_args.end(); j++) {
      assert(i < DAEMON_OPTS_SIZE);
      daemon_argv[i++] = const_cast<char *>(*j);
   }

   daemon_argv[i] = NULL;
   assert(i < DAEMON_OPTS_SIZE);

   daemon_argc = i;

   debug_printf2("Daemon CmdLine: ");
   for (int j = 0; j < daemon_argc; j++) {
      bare_printf2("%s ", daemon_argv[j]);
   }
   bare_printf2("\n");

   return spawnDaemon();
}

ForkLauncher *ForkLauncher::flauncher = NULL;
void on_child_forklauncher(int)
{
   ForkLauncher::flauncher->markFinished();
}

ForkLauncher::ForkLauncher(spindle_args_t *params_) :
   Launcher(params_),
   daemon_pid(0)
{
   signal(SIGCHLD, on_child_forklauncher);
   flauncher = this;
}

ForkLauncher::~ForkLauncher()
{
}

bool ForkLauncher::getReturnCodes(bool &daemon_done, int &daemon_ret,
                                  vector<pair<app_id_t, int> > &app_rets)
{
   for (;;) {
      int status, result;
      result = waitpid(-1, &status, WNOHANG);
      if (result == 0 || (result == -1 && errno == ECHILD)) {
         return true;
      }
      if (result == -1) {
         err_printf("Error calling waitpid: %s\n", strerror(errno));
         return false;
      }
      
      map<pid_t, app_id_t>::iterator appi = app_pids.find(result);
      if (WIFEXITED(status) && appi != app_pids.end()) {
         debug_printf("App process %d exited with code %d\n", result, (int) WEXITSTATUS(status));
         app_rets.push_back(make_pair(appi->second, WEXITSTATUS(status)));
         app_pids.erase(appi);
      }
      else if (WIFSIGNALED(status) && appi != app_pids.end()) {
         debug_printf("App process %d terminated with signal %d\n", result, WTERMSIG(status));
         app_rets.push_back(make_pair(appi->second, -1));
         app_pids.erase(appi);
      }
      else if (WIFEXITED(status) && result == daemon_pid) {
         daemon_done = true;
         daemon_ret = WEXITSTATUS(status);
         if (daemon_ret == 0) 
            debug_printf("Daemon process %d exited normally with exit code 0\n", daemon_pid);
         else
            err_printf("Daemon process %d exited with code %d\n", daemon_pid, daemon_ret);
      }
      else if (WIFSIGNALED(status) && result == daemon_pid) {
         err_printf("Daemon process %d terminated with signal %d\n", daemon_pid, WTERMSIG(status));
         daemon_done = true;
         daemon_ret = -1;
      }
      else {
         err_printf("Unexpected return from wait.  result = %d, status = %d (daemon_pid = %d)\n",
                    result, status, (int) daemon_pid);
         return false;
      }
   }
}

extern Launcher *createSlurmLauncher(spindle_args_t *params);
extern Launcher *createLSFLauncher(spindle_args_t *params);

Launcher *createMPILauncher(spindle_args_t *params)
{
   int launcher = params->use_launcher;

#if defined(TESTRM)
   if (!launcher || launcher == unknown_launcher || launcher == marker_launcher || 
       launcher == external_launcher)
   {
      if (strcmp(TESTRM, "slurm") == 0)
         launcher = srun_launcher;
      if (strcmp(TESTRM, "jsrun") == 0)
         launcher = jsrun_launcher;
      if (strcmp(TESTRM, "lrun") == 0)
         launcher = lrun_launcher;      
   }
#endif

   switch (launcher) {
      case srun_launcher:
         return createSlurmLauncher(params);
      case lrun_launcher:
      case jsrun_launcher:
         return createLSFLauncher(params);
      case openmpi_launcher:
      case wreckrun_launcher:
         err_printf("Unsupported launcher %d\n", launcher);
         fprintf(stderr, "Error: Spindle does not yet support this job launcher\n");
         exit(-1);
         break;
      default:
         err_printf("Could not identify a launcher: %d\n", launcher);
         fprintf(stderr, "Spindle Error: Could not identify system job launcher in command line\n");
         exit(-1);
         break;
   }
   return NULL;
}
