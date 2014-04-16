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

#include "lmon_api/common.h"
#include "lmon_api/lmon_proctab.h"
#include "lmon_api/lmon_fe.h"
#include "spindle_debug.h"
#include "config.h"

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

static void initLMONEnvironment()
{
   if (!strlen(LAUNCHMON_BIN_DIR))
      return;

   setenv("LMON_PREFIX", LAUNCHMON_BIN_DIR "/..", 0);
   setenv("LMON_LAUNCHMON_ENGINE_PATH", LAUNCHMON_BIN_DIR "/launchmon", 0);
   setenv("LMON_NEWLAUNCHMON_ENGINE_PATH", LAUNCHMON_BIN_DIR "/newlaunchmon", 0);
}

static const char *pt_to_string(const MPIR_PROCDESC_EXT &pt) { return pt.pd.host_name; }
bool rank_lt(const MPIR_PROCDESC_EXT &a, const MPIR_PROCDESC_EXT &b) { return a.mpirank < b.mpirank; }

struct str_lt {
   bool operator()(const char *a, const char *b) { return strcmp(a, b) < 0; }
};

static const char **getProcessTable(int aSession)
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

static pthread_mutex_t completion_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t completion_condvar = PTHREAD_COND_INITIALIZER;
static bool spindle_done = false;

static void signal_done()
{
   pthread_mutex_lock(&completion_lock);
   spindle_done = true;
   pthread_cond_signal(&completion_condvar);
   pthread_mutex_unlock(&completion_lock);
}

static void waitfor_done()
{
  pthread_mutex_lock(&completion_lock);
  while (!spindle_done) {
     pthread_cond_wait(&completion_condvar, &completion_lock);
  }
  pthread_mutex_unlock(&completion_lock);
}

static int onLaunchmonStatusChange(int *pstatus) {
   int status = *pstatus;
   if (WIFKILLED(status) || WIFDETACHED(status)) {
      signal_done();
   }
   return 0;
}

static int packfebe_cb(void *udata, 
                       void *msgbuf, 
                       int msgbufmax, 
                       int *msgbuflen)
{  
   spindle_args_t *args = (spindle_args_t *) udata;

   *msgbuflen = sizeof(unsigned int) * 2;
   *msgbuflen += sizeof(unique_id_t);
   assert(*msgbuflen < msgbufmax);
   
   char *buffer = (char *) msgbuf;
   int pos = 0;
   memcpy(buffer + pos, &args->port, sizeof(args->port));
   pos += sizeof(args->port);
   memcpy(buffer + pos, &args->num_ports, sizeof(args->num_ports));
   pos += sizeof(args->num_ports);
   memcpy(buffer + pos, &args->unique_id, sizeof(args->unique_id));
   pos += sizeof(args->unique_id);

   assert(pos == *msgbuflen);

   return 0;
}

int startLaunchmonFE(int app_argc, char *app_argv[],
                     int daemon_argc, char *daemon_argv[],
                     spindle_args_t *params)
{   
   int aSession, result;
   lmon_rc_e rc;

   initLMONEnvironment();

   rc = LMON_fe_init(LMON_VERSION);
   if (rc != LMON_OK )  {
      err_printf("[LMON FE] LMON_fe_init FAILED\n" );
      return EXIT_FAILURE;
   }

   rc = LMON_fe_createSession(&aSession);
   if (rc != LMON_OK)  {
      err_printf("[LMON FE] LMON_fe_createFEBESession FAILED\n");
      return EXIT_FAILURE;
   }
   
   rc = LMON_fe_regStatusCB(aSession, onLaunchmonStatusChange);
   if (rc != LMON_OK) {
      err_printf("[LMON FE] LMON_fe_regStatusCB FAILED\n");     
      return EXIT_FAILURE;
   }
   
   rc = LMON_fe_regPackForFeToBe(aSession, packfebe_cb);
   if (rc != LMON_OK) {
      err_printf("[LMON FE] LMON_fe_regPackForFeToBe FAILED\n");
      return EXIT_FAILURE;
   }

   rc = LMON_fe_launchAndSpawnDaemons(aSession, NULL,
                                      app_argv[0], app_argv,
                                      daemon_argv[0], daemon_argv+1,
                                      NULL, NULL);
   if (rc != LMON_OK) {
      err_printf("[LMON FE] LMON_fe_launchAndSpawnDaemons FAILED\n");
      return EXIT_FAILURE;
   }

   rc = LMON_fe_sendUsrDataBe(aSession, params);
   if (rc != LMON_OK) {
      err_printf("[LMON FE] Error sending user data to backends\n");
      return EXIT_FAILURE;
   }

   const char **hosts = getProcessTable(aSession);
   if (!hosts) {
      err_printf("[LMON FE] Error getting process table\n");
      return EXIT_FAILURE;
   }
   
   result = spindleInitFE(hosts, params);
   if (result == -1) {
      err_printf("[LMON FE] spindleInitFE returned an error\n");
      return EXIT_FAILURE;
   }
  
   waitfor_done();

   result = spindleCloseFE(params);
   if (result == -1) {
      err_printf("[LMON FE] spindleFinishFE returned an error\n");
      return EXIT_FAILURE;
   }

   return 0;
}
