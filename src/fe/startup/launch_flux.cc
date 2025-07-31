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

#include "fluxmgr.h"
#include "spindlemgr.h"

#include <string>
#include <vector>
#include <set>
#include <cassert>
#include <cstdlib>
#include <cstdio>

#include <flux/core.h>
#include <flux/hostlist.h>

using namespace std;

class FluxLauncher : public ForkLauncher
{
   friend Launcher *createFluxSessionLauncher(spindle_args_t *params, ConfigMap &config_);
private:
   bool initError;
   char **hostlist;
   static FluxLauncher *flauncher;
protected:
   virtual bool spawnDaemon();
   virtual bool spawnJob(app_id_t id, int app_argc, char **app_argv);
public:
   FluxLauncher(spindle_args_t *params_, ConfigMap &config_);
   virtual ~FluxLauncher();
   virtual const char **getProcessTable();
   virtual const char *getDaemonArg();
   virtual bool getReturnCodes(bool &daemon_done, int &daemon_ret,
                               std::vector<std::pair<app_id_t, int> > &app_rets);
   virtual void getSecondaryDaemonArgs(vector<const char *> &secondary_args);
   virtual bool handleShutdown();
   virtual bool getDefaultSessionID(string &session_id);
};

FluxLauncher *FluxLauncher::flauncher = NULL;

Launcher *createFluxSessionLauncher(spindle_args_t *params, ConfigMap &config)
{
   assert(!FluxLauncher::flauncher);
   FluxLauncher::flauncher = new FluxLauncher(params, config);
   if (FluxLauncher::flauncher->initError) {
      delete FluxLauncher::flauncher;
      FluxLauncher::flauncher = NULL;
   }
   return FluxLauncher::flauncher;
}

FluxLauncher::FluxLauncher(spindle_args_t *params_, ConfigMap &config_) :
   ForkLauncher(params_, config_),
   initError(false)
{
   int result;

   params->opts |= OPT_PERSIST;
   params->opts |= OPT_SESSION;
   
   result = fluxmgr_init();
   if (result == -1) {
      err_printf("Could not open connection to flux\n");
      initError = true;
      return;
   }
   hostlist = fluxmgr_get_hostlist();
   if (!hostlist) {
      err_printf("Could not get hostlist from flux\n");
      fluxmgr_close();
      initError = true;
      return;
   }
}

FluxLauncher::~FluxLauncher()
{
   fluxmgr_close();
   flauncher = NULL;
}

bool FluxLauncher::spawnDaemon()
{
   int result;
   int i, j, num_args, error;
   const char **flux_daemon_args;
   bool is_default_session = config.isSet(confStartSession);

   debug_printf2("Creating kvs spindle namespace in flux with daemon_args and bootstrap_args\n");
   result = setup_args_on_fe(params, is_default_session ? 1  : 0);
   if (result == -1) {
      err_printf("Failure setting up kvs parameters\n");
      fluxmgr_close();
      return false;
   }

   fluxmgr_close();

   if (params->opts & OPT_RSHLAUNCH) {
      //Rsh mode will launch daemons. Don't do flux exec launch
      return true;
   }
   
   debug_printf2("Launching spindle_be daemon with flux exec\n");
   i = 0;
   num_args = 5 + daemon_argc;
   flux_daemon_args = (const char **) malloc(sizeof(char *) * (num_args+1));
   flux_daemon_args[i++] = "flux";
   flux_daemon_args[i++] = "exec";
   flux_daemon_args[i++] = "--quiet";
   flux_daemon_args[i++] = "--noinput";
   flux_daemon_args[i++] = "--rank=all";
   for (j = 0; j < daemon_argc; j++) {
      flux_daemon_args[i++] = daemon_argv[j];
   }
   flux_daemon_args[i] = NULL;
   assert(i == num_args);

   daemon_pid = fork();
   if (daemon_pid == -1) {
      error = errno;
      err_printf("Error forking flux exec for daemon: %s\n", strerror(error));
      return false;
   }
   else if (daemon_pid == 0) {
      setenv("SPINDLE", "false", 1);
      execvp(flux_daemon_args[0], const_cast<char **>(flux_daemon_args));
      error = errno;
      err_printf("Failed to run 'flux exec': %s\n", strerror(error));
      fprintf(stderr, "Failed to run 'flux exec' to launch spindle daemons: %s\n", strerror(error));
      fprintf(stderr, "Attempted command line was: ");
      for (i = 0; flux_daemon_args[i]; i++) {
         fprintf(stderr, "%s ", flux_daemon_args[i]);
      }
      fprintf(stderr, "\n");      
      exit(-1);
   }
   debug_printf3("Spawned flux daemon with pid %d\n", (int) daemon_pid);
   return true;
}

bool FluxLauncher::spawnJob(app_id_t /*id*/, int /*app_argc*/, char ** /*app_argv*/)
{
   //Unsed for flux session launcher. Rather than launch jobs under spindle, we will
   // put spindle launch information in the flux KVS, then let the flux run spindle
   // plug-in capture that information during normal flux job runs.
   return false;
}

const char **FluxLauncher::getProcessTable()
{
   return const_cast<const char **>(hostlist);
}

const char *FluxLauncher::getDaemonArg()
{
   return "--spindle_mpi";
}

void FluxLauncher::getSecondaryDaemonArgs(vector<const char *> &secondary_args)
{
   if (params->opts & OPT_RSHLAUNCH)
      return;
   
   char port_str[32], ss_str[32], port_num_str[32];
   snprintf(port_str, 32, "%d", params->port);
   snprintf(port_num_str, 32, "%d", params->num_ports);
   snprintf(ss_str, 32, "%lu", params->unique_id);
   secondary_args.push_back(strdup(port_str));
   secondary_args.push_back(strdup(port_num_str));
   secondary_args.push_back(strdup(ss_str));
}

bool FluxLauncher::getReturnCodes(bool &daemon_done, int &daemon_ret,
                                   std::vector<std::pair<app_id_t, int> > &app_rets)
{
   if (!daemon_pid && (params->opts & OPT_RSHLAUNCH)) {
      daemon_pid = getRSHPidFE();
      markRSHPidReapedFE();
   }
   
   return ForkLauncher::getReturnCodes(daemon_done, daemon_ret, app_rets);
}

bool FluxLauncher::handleShutdown()
{
   int result;
   debug_printf2("Cleaning spindle namespace from flux KVS\n");
   result = fluxmgr_init();
   if (result == -1) {
      err_printf("Error initializing flux for shutdown\n");
      return false;
   }
   result = fluxmgr_rm_from_kvs();
   if (result == -1) {
      err_printf("Error removing spindle namespace from KVS\n");
      fluxmgr_close();      
      return false;
   }
   fluxmgr_close();      
   return true;
}

bool FluxLauncher::getDefaultSessionID(string &session_id)
{
   int result;
   char *session = NULL;

   result = fluxmgr_get_default_session(&session);
   if (result < 0) {
      debug_printf2("No default session set\n");
      return false;
   }

   debug_printf2("Using default session %s\n", session);
   session_id = string(session);
   return true;
}
