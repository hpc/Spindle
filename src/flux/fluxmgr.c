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

#include "fluxmgr.h"
#include "spindle_launch.h"

#include <flux/core.h>
#include <flux/hostlist.h>

#include <errno.h>
#include <string.h>

static flux_t *flux_handle;

#define SPINDLE_KVS_NAMESPACE "spindle"

int fluxmgr_init()
{
   int error;
   if (flux_handle)
      return 0;
   
   flux_handle = flux_open(NULL, 0);
   if (!flux_handle) {
      error = errno;
      spindle_debug_printf(1, "ERROR: Could not open flux handle: %s\n", strerror(error));
      return -1;
   }
   return 0;
}

void fluxmgr_close()
{
   if (flux_handle)
      flux_close(flux_handle);
   flux_handle = NULL;
}

int fluxmgr_is_headnode()
{
   static int is_head = -1;
   int needs_fluxmgr_close = 0, result, error;
   uint32_t rank;
   
   if (is_head != -1)
      return is_head;

   if (!flux_handle) {
      fluxmgr_init();
      needs_fluxmgr_close = 1;
   }

   result = flux_get_rank(flux_handle, &rank);
   if (needs_fluxmgr_close)
      fluxmgr_close();      
   if (result == -1) {
      error = errno;
      spindle_debug_printf(1, "ERROR: Could not get rank information: %s\n", strerror(error));
      return -1;
   }

   return (rank == 0) ? 1 : 0;
}

int fluxmgr_add_to_kvs(const char *daemon_args, const char *bootstrap_args, const char *default_session)
{
   flux_future_t *fresult;
   flux_kvs_txn_t *kvs_transaction;
   int result, error;
   
   spindle_debug_printf(2, "Creating kvs spindle namespace\n");
   fresult = flux_kvs_namespace_create(flux_handle, SPINDLE_KVS_NAMESPACE, FLUX_USERID_UNKNOWN, 0);
   if (!fresult) {
      spindle_debug_printf(1, "ERROR: Failed to create flux kvs namespace spindle\n");
      return -1;
   }
   result = flux_future_get(fresult, NULL);
   if (result == -1 && errno == EEXIST) {
      spindle_debug_printf(2, "Flux kvs namespace for spindle already exists. Not recreating\n");
   }
   else if (result == -1) {
      error = errno;
      spindle_debug_printf(1, "ERROR: Error creating flux kvs spindle namespace: %s\n", strerror(error));
      flux_future_destroy(fresult);      
      return -1;
   }
   flux_future_destroy(fresult);

   kvs_transaction = flux_kvs_txn_create();
   if (!kvs_transaction) {
      spindle_debug_printf(1, "ERROR: Failed to create kvs transaction\n");
      return -1;
   }
      
   result = flux_kvs_txn_put(kvs_transaction, 0, "daemon_args", daemon_args);
   if (result == -1) {
      spindle_debug_printf(1, "ERROR: Could not add daemon_args to kvs transaction\n");
      return -1;
   }
   spindle_debug_printf(2, "Setting flux kvs daemon_args = %s\n", daemon_args);

   result = flux_kvs_txn_put(kvs_transaction, 0, "bootstrap_args", bootstrap_args);
   if (result == -1) {
      spindle_debug_printf(1, "ERROR: Could not add bootstrap_args to kvs transaction\n");
      return -1;
   }
   spindle_debug_printf(2, "Set flux kvs boostrap_args = %s\n", bootstrap_args);

   if (default_session) {
      result = flux_kvs_txn_put(kvs_transaction, 0, "session", default_session);
      if (result == -1) {
         spindle_debug_printf(1, "ERROR: Could not add session to kvs transaction\n");
         return -1;
      }
      spindle_debug_printf(2, "Set flux kvs session = %s\n", default_session);
   }
   
   //Commit changes to kvs. These appearing will trigger the daemons to start
   spindle_debug_printf(2, "Commiting daemon_args and bootstrap_args to kvs\n");
   fresult = flux_kvs_commit(flux_handle, SPINDLE_KVS_NAMESPACE, 0, kvs_transaction);
   if (!fresult) {
      spindle_debug_printf(1, "ERROR: Could not commit new keys to spindle kvs\n");
      return -1;
   }
   result = flux_future_get(fresult, NULL);
   if (result == -1) {
      error = errno;
      spindle_debug_printf(1, "ERROR: Committing new keys: %s\n", strerror(error));
      flux_future_destroy(fresult);      
      return -1;
   }
   flux_future_destroy(fresult);
   
   return 0;
}

#define GET_FROM_KVS_TIMEOUT -3
#define GET_FROM_KVS_NO_NAMESPACE -2
#define GET_FROM_KVS_ERR -1
static int get_from_kvs(flux_t *fhandle, const char *name, char **value, int waitfor_timeout)
{
   flux_future_t *fresult = NULL;
   int ret, error, result, do_fluxmgr_close = 0;
   const char *data = NULL;
   
   if (!fhandle)
      fhandle = flux_handle;
   if (!fhandle) {
      fluxmgr_init();
      do_fluxmgr_close = 1;
      fhandle = flux_handle;
   }
   
   fresult = flux_kvs_lookup(fhandle, SPINDLE_KVS_NAMESPACE, waitfor_timeout ? FLUX_KVS_WAITCREATE : 0, name);
   if (!fresult) {
      error = errno;
      spindle_debug_printf(1, "ERROR: Unable to lookup spindle %s in flux kvs: %s\n", name, strerror(error));
      ret = GET_FROM_KVS_ERR;
      goto done;
   }

   if (waitfor_timeout) {
      result = flux_future_wait_for(fresult, (double) waitfor_timeout);
      if (result == -1 && errno == ETIMEDOUT) {
         spindle_debug_printf(1, "ERROR: Timed out in waiting for spindle %s\n", name);
         ret = GET_FROM_KVS_TIMEOUT;
         goto done;
      }
      if (result == -1) {
         error = errno;
         spindle_debug_printf(1, "ERROR: Error in flux_future_wait_for on %s: %s\n", name, strerror(error));
         ret = GET_FROM_KVS_ERR;
         goto done;
      }      
   }
   
   result = flux_kvs_lookup_get(fresult, &data);
   if (result == -1 && (errno == ENOENT || errno == EOPNOTSUPP)) {
      spindle_debug_printf(2, "Flux sessions are not not enabled\n");
      ret = GET_FROM_KVS_NO_NAMESPACE;
      goto done;
   }
   else if (result == -1) {
      error = errno;
      spindle_debug_printf(1, "ERROR: Error in flux_kvs_lookup_get for %s: %s\n", name, strerror(error));
      ret = GET_FROM_KVS_ERR;
      goto done;
   }

   if (value)
      *value = strdup(data);

   ret = 0;
  done:
   if (fresult)
      flux_future_destroy(fresult);
   if (do_fluxmgr_close)
      fluxmgr_close();
   
   return ret;
}

int fluxmgr_get_from_kvs(char **daemon_args, int timeout_seconds)
{
   int result;
   result = get_from_kvs(NULL, "daemon_args", daemon_args, timeout_seconds);
   if (result == GET_FROM_KVS_ERR || result == GET_FROM_KVS_NO_NAMESPACE)
      return -1;
   else if (result == GET_FROM_KVS_TIMEOUT)
      return -2;
   else
      return 0;
}

int fluxmgr_get_bootstrap(flux_t *fhandle, char **bootstrap_args)
{
   fhandle=fhandle;
   int result;
   result = get_from_kvs(NULL, "bootstrap_args", bootstrap_args, 0);
   if (result == GET_FROM_KVS_ERR || result == GET_FROM_KVS_TIMEOUT)
      return -1;
   else if (result == GET_FROM_KVS_NO_NAMESPACE)
      return -2;
   else
      return 0;
}

int fluxmgr_get_default_session(char **default_session)
{
   int result;
   result = get_from_kvs(NULL, "session", default_session, 0);
   if (result == GET_FROM_KVS_ERR || result == GET_FROM_KVS_TIMEOUT)
      return -1;
   else if (result == GET_FROM_KVS_NO_NAMESPACE)
      return -2;
   else
      return 0;
}

int fluxmgr_waitfor(int timeout)
{
   return fluxmgr_get_from_kvs(NULL, timeout);
}

int fluxmgr_rm_from_kvs()
{
   flux_future_t *fresult;
   int error, result;

   fresult = flux_kvs_namespace_remove(flux_handle, SPINDLE_KVS_NAMESPACE);
   if (!fresult) {
      error = errno;
      spindle_debug_printf(1, "ERROR: Could not remove %s namespace: %s\n", SPINDLE_KVS_NAMESPACE, strerror(error));
      return -1;
   }
   result = flux_future_get(fresult, NULL);
   if (result == -1) {
      error = errno;
      spindle_debug_printf(1, "ERROR: Could not remove %s namespace: %s\n", SPINDLE_KVS_NAMESPACE, strerror(error));
      flux_future_destroy(fresult);      
      return -1;
   }
   flux_future_destroy(fresult);
   return 0;
}

char **fluxmgr_get_hostlist()
{
   static char **hosts = NULL;
   const char *hostlist_short;
   struct hostlist *hl;
   int count, cur;

   if (hosts)
      return hosts;
   
   hostlist_short = flux_attr_get(flux_handle, "hostlist");
   if (!hostlist_short) {
      spindle_debug_printf(1, "ERROR: Could not get attr hostlist from flux\n");
      return NULL;
   }

   hl = hostlist_create();
   hostlist_append(hl, hostlist_short);
   
   cur = 0;
   count = hostlist_count(hl);
   hosts = (char **) malloc(sizeof(char *) * (count+1));   
   for (const char *h = hostlist_first(hl); h; h = hostlist_next(hl)) {
      hosts[cur++] = strdup(h);
   }
   hosts[cur] = NULL;
   hostlist_destroy(hl);
   spindle_debug_printf(2, "Returning host list with %d hosts\n", cur);
   return hosts;
}

void fluxmgr_free_hostlist(char **hostlist)
{
   for (char **i = hostlist; *i != NULL; i++) {
      free(*i);
   }
   free(hostlist);
}
