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

#include <set>
#include "../startup/launcher.h"

#include "lmon_api/common.h"
#include "lmon_api/lmon_proctab.h"
#include "lmon_api/lmon_fe.h"

#include "spindle_debug.h"
#include "config.h"
#include "spindle_launch.h"

#include <set>
#include <algorithm>
#include <string>
#include <iterator>
#include <pthread.h>
#include <cassert>
#include <vector>
#include "spindle_launch.h"

using namespace std;

#if !defined(LAUNCHMON_BIN_DIR)
#error Expected LAUNCHMON_BIN_DIR to be defined
#endif

#if defined(os_bluegene)
#define PUSH_ENV_DEFAULT_VAL true
#else
#define PUSH_ENV_DEFAULT_VAL false
#endif


static bool push_env = PUSH_ENV_DEFAULT_VAL;
extern char **environ;


class LMonLauncher : public Launcher 
{
   friend Launcher *createLaunchmonLauncher(spindle_args_t *params);
private:
   static LMonLauncher *llauncher;
   int aSession;
   app_id_t appid;
   int status;
   bool initError;

   void initEnvironment();
   bool initLMon();
   LMonLauncher(spindle_args_t *params_);
   static int pack_environ(void *udata, void *msgbuf, 
                           int msgbufmax, int *msgbuflen);
   static int packfebe_cb(void *udata, void *msgbuf, 
                          int msgbufmax, int *msgbuflen);
   static int onLaunchmonStatusChange_cb(int *pstatus);
protected:
   virtual bool spawnDaemon();
public:
   virtual bool spawnJob(app_id_t id, int app_argc, char **app_argv);
   virtual const char **getProcessTable();
   virtual const char *getDaemonArg();
   virtual bool getReturnCodes(bool &daemon_done, int &daemon_ret,
                               vector<pair<app_id_t, int> > &app_rets);
   int onLaunchmonStatusChange(int *pstatus);
};

LMonLauncher *LMonLauncher::llauncher = NULL;

Launcher *createLaunchmonLauncher(spindle_args_t *params)
{
   LMonLauncher *l = new LMonLauncher(params);
   if (l->initError) {
      delete l;
      return NULL;
   }
   LMonLauncher::llauncher = l;
   return l;
}

void LMonLauncher::initEnvironment()
{
   if (!strlen(LAUNCHMON_BIN_DIR))
      return;

   setenv("LMON_PREFIX", LAUNCHMON_BIN_DIR "/..", 0);
   setenv("LMON_LAUNCHMON_ENGINE_PATH", LAUNCHMON_BIN_DIR "/launchmon", 0);
   setenv("LMON_NEWLAUNCHMON_ENGINE_PATH", LAUNCHMON_BIN_DIR "/newlaunchmon", 0);
#if defined(os_bluegene)
   setenv("LMON_DONT_STOP_APP", "1", 1);
#endif
}

static const char *pt_to_string(const MPIR_PROCDESC_EXT &pt) { 
   return pt.pd.host_name; 
}

static bool rank_lt(const MPIR_PROCDESC_EXT &a, const MPIR_PROCDESC_EXT &b) { 
   return a.mpirank < b.mpirank; 
}

struct str_lt {
   bool operator()(const char *a, const char *b) { return strcmp(a, b) < 0; }
};

const char **LMonLauncher::getProcessTable()
{
  /* Get the process table */
   lmon_rc_e rc;
   unsigned int ptable_size, actual_size;

   rc = LMON_fe_getProctableSize(aSession, &ptable_size);
   if (rc != LMON_OK) {
      err_printf("LMON_fe_getProctableSize failed\n");
      return NULL;
   }
   MPIR_PROCDESC_EXT *proctab = (MPIR_PROCDESC_EXT *) malloc(sizeof(MPIR_PROCDESC_EXT) * ptable_size);
   rc = LMON_fe_getProctable(aSession, proctab, &actual_size, ptable_size);
   if (rc != LMON_OK) {
      err_printf("LMON_fe_getProctable failed\n");
      return NULL;
   }

   /* Transform the process table to a list of hosts.  Pass through std::set to make hostnames unique */
   set<const char *, str_lt > host_set;
   transform(proctab, proctab+ptable_size, inserter(host_set, host_set.begin()), pt_to_string);
   size_t hosts_size = host_set.size();
   const char **hosts = static_cast<const char **>(malloc((hosts_size+1) * sizeof(char *)));
   copy(host_set.begin(), host_set.end(), hosts);
   hosts[hosts_size] = NULL;

   /* Swap the hostname containing rank 0 to the front of the list so it'll be the leader */
   MPIR_PROCDESC_EXT *smallest_rank = std::min_element(proctab, proctab+ptable_size, rank_lt);
   const char **smallest_host = find(hosts, hosts + hosts_size, smallest_rank->pd.host_name);
   std::swap(*hosts, *smallest_host);

   free(proctab);
   return hosts;
}

const char *LMonLauncher::getDaemonArg()
{
   return "--spindle_lmon";
}

bool LMonLauncher::getReturnCodes(bool &daemon_done, int &daemon_ret,
                                  vector<pair<app_id_t, int> > &app_rets)
{
   if (WIFKILLED(status) || WIFDETACHED(status)) {
      daemon_done = true;
      daemon_ret = 0;
      app_rets.push_back(make_pair(appid, 0));
   }
   return true;
}

int LMonLauncher::onLaunchmonStatusChange_cb(int *pstatus)
{
   return llauncher->onLaunchmonStatusChange(pstatus);
}

int LMonLauncher::onLaunchmonStatusChange(int *pstatus) {
   assert(llauncher);
   if (WIFKILLED(*pstatus) || WIFDETACHED(*pstatus)) {
      llauncher->status = *pstatus;
      llauncher->markFinished();
   }
   return 0;
}

int LMonLauncher::pack_environ(void *udata, void *msgbuf, 
                               int msgbufmax, int *msgbuflen)
{
   size_t pos = 0;
   char *buffer = (char *) msgbuf;

   for (char **cur_env = (char **) udata; *cur_env != NULL; cur_env++) {
      char *env = *cur_env;
      size_t len = strlen(env);
      assert(pos + len + 1 < (size_t) msgbufmax);
      
      memcpy(buffer + pos, env, len);
      pos += len;
      
      buffer[pos] = '\n';
      pos += 1;
   }
   if (pos > 0)
      buffer[pos-1] = '\0';

   *msgbuflen = (int) pos;
   return 0;
}

int LMonLauncher::packfebe_cb(void *udata, void *msgbuf, 
                       int msgbufmax, int *msgbuflen)
{  
   if (udata == (void *) environ)
      return pack_environ(udata, msgbuf, msgbufmax, msgbuflen);

   spindle_args_t *args = (spindle_args_t *) udata;
   unsigned int send_env = push_env ? 1 : 0;

   *msgbuflen = sizeof(unsigned int) * 3;
   *msgbuflen += sizeof(unique_id_t);
   assert(*msgbuflen < msgbufmax);
   
   char *buffer = (char *) msgbuf;
   int pos = 0;
   memcpy(buffer + pos, &args->port, sizeof(args->port));
   pos += sizeof(args->port);
   memcpy(buffer + pos, &args->num_ports, sizeof(args->num_ports));
   pos += sizeof(args->num_ports);
   memcpy(buffer + pos, &send_env, sizeof(send_env));
   pos += sizeof(send_env);
   memcpy(buffer + pos, &args->unique_id, sizeof(args->unique_id));
   pos += sizeof(args->unique_id);

   assert(pos == *msgbuflen);

   return 0;
}

LMonLauncher::LMonLauncher(spindle_args_t *params_) :
   Launcher(params_),
   aSession(0),
   appid(0),
   status(0),
   initError(false)
{
   initEnvironment();
   if (!initLMon()) 
      initError = true;
}

bool LMonLauncher::initLMon() {
   lmon_rc_e rc;

   rc = LMON_fe_init(LMON_VERSION);
   if (rc != LMON_OK )  {
      err_printf("[LMON FE] LMON_fe_init FAILED\n" );
      return false;
   }

   rc = LMON_fe_createSession(&aSession);
   if (rc != LMON_OK)  {
      err_printf("[LMON FE] LMON_fe_createFEBESession FAILED\n");
      return false;
   }
   
   rc = LMON_fe_regStatusCB(aSession, onLaunchmonStatusChange_cb);
   if (rc != LMON_OK) {
      err_printf("[LMON FE] LMON_fe_regStatusCB FAILED\n");     
      return false;
   }
   
   rc = LMON_fe_regPackForFeToBe(aSession, packfebe_cb);
   if (rc != LMON_OK) {
      err_printf("[LMON FE] LMON_fe_regPackForFeToBe FAILED\n");
      return false;
   }
   return true;
}

bool LMonLauncher::spawnDaemon()
{
   assert(daemon_argc > 0);
   assert(daemon_argv);
   return true;
}

bool LMonLauncher::spawnJob(app_id_t id, int app_argc, char **app_argv)
{   
   lmon_rc_e rc;

   rc = LMON_fe_launchAndSpawnDaemons(aSession, NULL,
                                      app_argv[0], app_argv,
                                      daemon_argv[0], daemon_argv+1,
                                      NULL, NULL);
   if (rc != LMON_OK) {
      err_printf("[LMON FE] LMON_fe_launchAndSpawnDaemons FAILED\n");
      return false;
   }

   rc = LMON_fe_sendUsrDataBe(aSession, params);
   if (rc != LMON_OK) {
      err_printf("[LMON FE] Error sending user data to backends\n");
      return false;
   }

   if (push_env) {
      rc = LMON_fe_sendUsrDataBe(aSession, environ);
      if (rc != LMON_OK) {
         err_printf("[LMON FE] Error sending environment data to backends\n");
         return false;
      }
   }

   appid = id;
   return true;
}
