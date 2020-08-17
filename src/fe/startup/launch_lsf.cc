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

#include <string>
#include <vector>
#include <cassert>

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

class LSFLauncher : public ForkLauncher
{
   friend Launcher *createLSFLauncher(spindle_args_t *params);
private:
   bool initError;
   static LSFLauncher *llauncher;

   const char *getNextLSBHost(const char *prev);
   static void countHost(const char *host, void *data, int);
   static void parseHost(const char *hosts, void *data, int cur_index);
   void forEachLSBHost(const char *hosts, void (*cb)(const char *, void *, int), void *data);   
protected:
   virtual bool spawnDaemon();
public:
   LSFLauncher(spindle_args_t *params);
   virtual ~LSFLauncher();
   virtual const char **getProcessTable();
   virtual const char *getDaemonArg();
   virtual bool spawnJob(app_id_t id, int app_argc, char **app_argv);
   virtual bool getReturnCodes(bool &daemon_done, int &daemon_ret,
                               std::vector<std::pair<app_id_t, int> > &app_rets);
};

LSFLauncher *LSFLauncher::llauncher = NULL;

Launcher *createLSFLauncher(spindle_args_t *params)
{
   assert(!LSFLauncher::llauncher);
   LSFLauncher::llauncher = new LSFLauncher(params);
   if (LSFLauncher::llauncher->initError) {
      delete LSFLauncher::llauncher;
      LSFLauncher::llauncher = NULL;
   }
   return LSFLauncher::llauncher;
}

LSFLauncher::LSFLauncher(spindle_args_t *params_) :
   ForkLauncher(params_),
   initError(false)
{
   char *mcpu_hosts = getenv("LSB_MCPU_HOSTS");
   if (!mcpu_hosts) {
      err_printf("LSB_MCPU_HOSTS environment variable not set\n");
      fprintf(stderr, "Spindle Error: Not running inside an LSF allocation (LSB_MCPU_HOSTS not set)\n");
      initError = true;      
   }
   params->opts |= OPT_RSHLAUNCH;
}

LSFLauncher::~LSFLauncher()
{
}

const char *LSFLauncher::getNextLSBHost(const char *prev)
{
   if (prev == NULL)
      return NULL;
   while (*prev != ' ' && *prev != '\0')
      prev++;
   if (*prev == '\0')
      return NULL;
   while (*prev == ' ' && *prev != '\0')
      prev++;
   if (*prev == '\0')
      return NULL;
   return prev;
}

void LSFLauncher::countHost(const char *host, void *data, int)
{
   int *count = (int *) data;
   (*count)++;
}

void LSFLauncher::parseHost(const char *hosts, void *data, int cur_index)
{
   int hostname_len = 0;
   while (hosts[hostname_len] != ' ' && hosts[hostname_len] != '\0') {
      hostname_len++;
   }

   char *hostname = (char *) malloc(hostname_len+1);
   strncpy(hostname, hosts, hostname_len);
   hostname[hostname_len] = '\0';

   char **loc = (char **) data;
   loc[cur_index] = hostname;
}

void LSFLauncher::forEachLSBHost(const char *hosts, void (*cb)(const char *, void *, int), void *data)
{
   const char *cur;
   int cur_index = 0;
   
   cur = getNextLSBHost(hosts); //Skip launch host
   for (;;) {
      cur = getNextLSBHost(cur); //Skip proc count
      if (!cur) return;
      cb(cur, data, cur_index++);
      cur = getNextLSBHost(cur);
   }
}

const char **LSFLauncher::getProcessTable()
{
   char *mcpu_hosts = getenv("LSB_MCPU_HOSTS");
   int hostcount = 0;
   char **hostlist;

   forEachLSBHost(mcpu_hosts, countHost, &hostcount);

   hostlist = (char **) malloc(sizeof(char*) * (hostcount+1));
   forEachLSBHost(mcpu_hosts, parseHost, (void*) hostlist);
   hostlist[hostcount] = NULL;

   return (const char **) hostlist;
}

const char *LSFLauncher::getDaemonArg()
{
   return "--spindle_lsflaunch";
}


bool LSFLauncher::spawnDaemon()
{
   return true;
}

bool LSFLauncher::spawnJob(app_id_t id, int app_argc, char **app_argv)
{
   debug_printf("Spindle launching LSF job with app-id %lu: %s\n", id, app_argv[0]);
   int pid = fork();
   if (pid == -1) {
      int error = errno;
      err_printf("Failed to fork process for lsf app: %s\n", strerror(error));
      return false;
   }
   else if (pid == 0) {
      execvp(*app_argv, app_argv);
      int error = errno;
      fprintf(stderr, "Spindle failed to run %s: %s\n", app_argv[0], strerror(error));
      err_printf("Failed to run application %s: %s\n", app_argv[0], strerror(error));
      exit(-1);
   }
   app_pids[pid] = id;
   return true;   
}

bool LSFLauncher::getReturnCodes(bool &daemon_done, int &daemon_ret,
                                 std::vector<std::pair<app_id_t, int> > &app_rets)
{
   daemon_pid = getRSHPidFE();   
   ForkLauncher::getReturnCodes(daemon_done, daemon_ret, app_rets);
   markRSHPidReapedFE();
   return true;
}

