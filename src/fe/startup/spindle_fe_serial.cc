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
#include "launcher.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <map>

using namespace std;

class SerialLauncher : public ForkLauncher
{
   friend Launcher *createSerialLauncher(spindle_args_t *params);
private:
   bool initError;
   pid_t daemon_pid;
   static SerialLauncher *slauncher;

   SerialLauncher(spindle_args_t *params_);
protected:
   virtual bool spawnDaemon();
public:
   virtual bool spawnJob(app_id_t id, int app_argc, char **app_argv);
   virtual const char **getProcessTable();
   virtual const char *getDaemonArg();
   virtual ~SerialLauncher();
};
SerialLauncher *SerialLauncher::slauncher = NULL;

Launcher *createSerialLauncher(spindle_args_t *params)
{
   SerialLauncher::slauncher = new SerialLauncher(params);
   if (SerialLauncher::slauncher->initError) {
      delete SerialLauncher::slauncher;
      return NULL;
   }
   return static_cast<Launcher*>(SerialLauncher::slauncher);
}

SerialLauncher::SerialLauncher(spindle_args_t *params_) :
   ForkLauncher(params_),
   initError(false)
{
}

SerialLauncher::~SerialLauncher()
{
}

bool SerialLauncher::spawnJob(app_id_t id, int /*app_argc*/, char *app_argv[])
{
   pid_t app_pid = fork();
   if (app_pid == -1) {
      int error = errno;
      err_printf("Error forking application: %s\n", strerror(error));
      return false;
   }
   else if (app_pid != 0) {
      debug_printf2("Forked application pid %d\n", app_pid);
      app_pids[app_pid] = id;
      return true;
   }
   else {
      execvp(app_argv[0], app_argv);
      int error = errno;
      err_printf("Error exec'ing application %s: %s\n", app_argv[0], strerror(error));
      fprintf(stderr, "Error running %s: %s\n", app_argv[0], strerror(error)); 
      exit(-1);
      return false;
   }
}
   
bool SerialLauncher::spawnDaemon()
{
   daemon_pid = fork();
   if (daemon_pid == -1) {
      int error = errno;
      err_printf("Error forking daemon: %s\n", strerror(error));
      return false;
   }
   else if (daemon_pid != 0) {
      debug_printf("Forked daemon pid %d\n", daemon_pid);
      return true;
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
      int error = errno;
      err_printf("Error exec'ing daemon %s: %s\n", daemon_argv[0], strerror(error));
      exit(-1);
      return false;
   }
}

const char **SerialLauncher::getProcessTable()
{
   const char **hostlist;
   hostlist = (const char **) malloc(sizeof(const char *) * 2);
   hostlist[0] = "localhost";
   hostlist[1] = NULL;
   return hostlist;
}

const char *SerialLauncher::getDaemonArg()
{
   return "--spindle_serial";
}
