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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/inotify.h>
#include <errno.h>
#include <assert.h>

#include "ldcs_api.h"
#include "ldcs_api_listen.h"
#include "ldcs_audit_server_process.h"
#include "ldcs_audit_server_filemngt.h"
#include "ldcs_audit_server_numa.h"
#include "ldcs_audit_server_md.h"
#include "ldcs_cache.h"
#include "stat_cache.h"
#include "global_name.h"
#include "ldcs_audit_server_handlers.h"
#include "ldcs_audit_server_requestors.h"
#include "spindle_launch.h"
#include "pathfn.h"
#include "msgbundle.h"
#include "ccwarns.h"
#include "parse_mounts.h"
#include "exitnote.h"

/** 
 * This file contains the "brains" of Spindle.  It's public interface,
 * declared in the handlers header, is invoked when a packet is received
 * from a client or other server.  These functions will then decode that 
 * packet and decide how to respond.  Those responses could be things like:
 * adding a file's contents to the cache, reading a file/directory from disk,
 * requesting a file from the network, broadcasting a directory's contents,
 * storing a file in the localdisk, send a load path to a client, etc...
 *
 * In response to messages, these functions will trigger IO operations in the 
 * form of filemngt, network calls in the server_md interface, client calls
 * via the ldcs_api.h, or cache calls via ldcs_cache.h.
 **/
typedef enum {
   FOUND_FILE,
   FOUND_ERRCODE,
   NO_FILE,
   NO_DIR,
   READ_DIRECTORY,
   READ_FILE,
   REQ_DIRECTORY,
   REQ_FILE,
   ALIAS_TO,
   ORIG_FILE
} handle_file_result_t;

typedef enum {
   REQUEST_METADATA,
   METADATA_FILE,
   REPORT_METADATA,
   METADATA_IN_PROGRESS
} handle_metadata_result_t;

typedef enum {
   preload_broadcast,
   request_broadcast,
   suppress_broadcast
} broadcast_t;

typedef enum {
   exists,
   not_exists
} exist_t;

typedef enum {
   is_elf_yes,
   is_elf_no,
   is_elf_unknown
} is_elf_t;
#define SPINDLE_ENODIR -68

static int handle_client_info_msg(ldcs_process_data_t *procdata, int nc, ldcs_message_t *msg);
static int handle_client_myrankinfo_msg(ldcs_process_data_t *procdata, int nc, ldcs_message_t *msg);
static int handle_pythonprefix_query(ldcs_process_data_t *procdata, int nc);
static int handle_client_file_request(ldcs_process_data_t *procdata, int nc, ldcs_message_t *msg);
static handle_file_result_t handle_howto_directory(ldcs_process_data_t *procdata, char *dir);
static handle_file_result_t handle_howto_file(ldcs_process_data_t *procdata, char *pathname,
                                              char *file, char *dir, char **localpath, char **aliasto, int *errcode);
static handle_metadata_result_t handle_howto_metadata(ldcs_process_data_t *procdata, char *pathname, metadata_t mdtype);
static int handle_client_progress(ldcs_process_data_t *procdata, int nc);
static int handle_progress(ldcs_process_data_t *procdata);
static int handle_client_get_alias(ldcs_process_data_t *procdata, int nc, char *alias_to);
static int handle_read_directory(ldcs_process_data_t *procdata, char *dir);
static int handle_broadcast_dir(ldcs_process_data_t *procdata, char *dir, broadcast_t bcast);
static int handle_read_and_broadcast_dir(ldcs_process_data_t *procdata, char *dir);

static int handle_read_and_broadcast_file(ldcs_process_data_t *procdata, char *filename, 
                                          broadcast_t bcast);
static int handle_broadcast_file(ldcs_process_data_t *procdata, char *pathname, char *buffer, size_t size,
                                 broadcast_t bcast);
static void *handle_setup_file_buffer(ldcs_process_data_t *procdata, char *pathname, size_t size,
                                      int *fd, char **localpath, int *already_loaded, int *replicate,
                                      is_elf_t is_elf);
static int handle_finish_buffer_setup(ldcs_process_data_t *procdata, 
                                      char *localname, char *pathname, int *fd,
                                      char **origbuffer, size_t size, size_t newsize,
                                      int *replicatep, int errcode);

static int handle_client_originalfile_query(ldcs_process_data_t *procdata, int nc);
static int handle_client_fulfilled_query(ldcs_process_data_t *procdata, int nc);
static int handle_client_return_numa_replication(ldcs_process_data_t *procdata, int nc);
static int handle_client_rejected_query(ldcs_process_data_t *procdata, int nc, int errcode);

static int handle_request(ldcs_process_data_t *procdata, node_peer_t from, ldcs_message_t *msg);
static int handle_request_directory(ldcs_process_data_t *procdata, node_peer_t from, char *pathname);
static int handle_request_file(ldcs_process_data_t *procdata, node_peer_t from, char *pathname);

static int handle_send_query(ldcs_process_data_t *procdata, char *path, int is_dir);
static int handle_send_directory_query(ldcs_process_data_t *procdata, char *directory);
static int handle_send_file_query(ldcs_process_data_t *procdata, char *fullpath);

static int handle_file_recv(ldcs_process_data_t *procdata, ldcs_message_t *msg, node_peer_t peer, 
                            broadcast_t bcast);
static int handle_directory_recv(ldcs_process_data_t *procdata, ldcs_message_t *msg, broadcast_t bcast);
static int handle_alias_recv(ldcs_process_data_t *procdata, ldcs_message_t *msg, broadcast_t bcast);

static int handle_exit_broadcast(ldcs_process_data_t *procdata);
static int handle_send_msg_to_keys(ldcs_process_data_t *procdata, ldcs_message_t *msg, char *key,
                                   void *secondary_data, size_t secondary_size, int force_broadcast,
                                   metadata_t mdtype);
static int handle_preload_filelist(ldcs_process_data_t *procdata, ldcs_message_t *msg);
static int handle_preload_done(ldcs_process_data_t *procdata);
static int handle_create_selfload_file(ldcs_process_data_t *procdata, char *filename);
static int handle_recv_selfload_file(ldcs_process_data_t *procdata, ldcs_message_t *msg);
static int handle_report_fileexist_result(ldcs_process_data_t *procdata, int nc, exist_t res);

static int handle_fileexist_test(ldcs_process_data_t *procdata, int nc);
static int handle_client_fileexist_msg(ldcs_process_data_t *procdata, int nc, ldcs_message_t *msg);
static int handle_client_origpath_msg(ldcs_process_data_t *procdata, int nc, ldcs_message_t *msg);
static int handle_stat_file(ldcs_process_data_t *procdata, char *pathname, metadata_t mdtype, char **localname, struct stat *buf);
static int handle_metadata_and_broadcast_file(ldcs_process_data_t *procdata, char *pathname, metadata_t mdtype, broadcast_t bcast);
static int handle_cache_metadata(ldcs_process_data_t *procdata, char *pathname, int file_exists, metadata_t mdtype,
                                 struct stat *buf, char **localname);
static int handle_broadcast_metadata(ldcs_process_data_t *procdata, char *pathname, int file_exists, unsigned char *buf, size_t buf_size, metadata_t mdtype);
static int handle_broadcast_errorcode(ldcs_process_data_t *procdata, char *pathname, int errcode);
static int handle_broadcast_alias(ldcs_process_data_t *procdata, char *alias_from, char *alias_to);
static int handle_metadata_recv(ldcs_process_data_t *procdata, ldcs_message_t *msg, metadata_t mdtype, node_peer_t peer);
static int handle_client_metadata(ldcs_process_data_t *procdata, int nc);
static int handle_client_metadata_result(ldcs_process_data_t *procdata, int nc, metadata_t mdtype);
static int handle_metadata_request(ldcs_process_data_t *procdata, char *pathname, metadata_t mdtype, node_peer_t from);
static int handle_metadata_request_recv(ldcs_process_data_t *procdata, ldcs_message_t *msg, metadata_t mdtype, node_peer_t peer);
static int handle_load_and_broadcast_metadata(ldcs_process_data_t *procdata, char *pathname, metadata_t mdtype);
static int handle_send_exit_ready_if_done(ldcs_process_data_t *procdata);
static int handle_exit_ready_msg(ldcs_process_data_t *procdata, ldcs_message_t *msg);
static int handle_exit_cancel_msg(ldcs_process_data_t *procdata, ldcs_message_t *msg);
static int handle_send_exit_cancel(ldcs_process_data_t *procdata);
static int handle_read_ldso_metadata(ldcs_process_data_t *procdata, char *pathname, ldso_info_t *ldsoinfo, char **result_file);
static int handle_cache_ldso(ldcs_process_data_t *procdata, char *pathname, int file_exists,
                             ldso_info_t *ldsoinfo, char **localname);
static int handle_msgbundle(ldcs_process_data_t *procdata, node_peer_t peer, ldcs_message_t *msg);
static int handle_setup_alias(ldcs_process_data_t *procdata, char *pathname, char *alias_to);
static int handle_close_client_query(ldcs_process_data_t *procdata, int nc);

/**
 * Query from client to server.  Returns info about client's rank in server data structures. 
 **/
static int handle_client_info_msg(ldcs_process_data_t *procdata, int nc, ldcs_message_t *msg)
{
   ldcs_client_t *client = procdata->client_table + nc;
   if(msg->header.type == LDCS_MSG_CWD) {
      strncpy(client->remote_cwd, msg->data, sizeof(client->remote_cwd)-1);
      client->remote_cwd[sizeof(client->remote_cwd)-1] = '\0';
      debug_printf2("Server recvd CWD %s from %d\n", msg->data, nc);
   } 
   else if(msg->header.type == LDCS_MSG_PID) {
      int mypid;
      sscanf(msg->data,"%d",&mypid);
      client->remote_pid=mypid;
      debug_printf2("Server recvd pid %d from %d\n", mypid, nc);
   } 
   else if(msg->header.type == LDCS_MSG_LOCATION) {
      strncpy(client->remote_location, msg->data, sizeof(client->remote_location)-1);
      client->remote_location[sizeof(client->remote_location)-1] = '\0';
      debug_printf2("Server recvd location %s from %d\n", msg->data, nc);
   }
   else if (msg->header.type == LDCS_MSG_CPU) {
      int clientcpu;
      sscanf(msg->data, "%d", &clientcpu);
      client->numa_node = numa_node_for_core(clientcpu);
      if (client->numa_node == -1)
         client->numa_node = 0;
      debug_printf2("Server recvd cpu info from client on %d, marking as numa domain %d\n", clientcpu, client->numa_node);
   }
   return 0;
}

static int handle_pythonprefix_query(ldcs_process_data_t *procdata, int nc)
{
   ldcs_message_t msg;
   int connid;
   ldcs_client_t *client;

   assert(nc != -1);
   client = procdata->client_table + nc;
   connid = client->connid;
   if (client->state != LDCS_CLIENT_STATUS_ACTIVE || connid < 0)
      return 0;

   msg.header.type = LDCS_MSG_PYTHONPREFIX_RESP;
   msg.header.len = strlen(procdata->pythonprefix) + 1;
   msg.data = procdata->pythonprefix;
   
   ldcs_send_msg(connid, &msg);
   procdata->server_stat.clientmsg.cnt++;
   procdata->server_stat.clientmsg.time += ldcs_get_time() - client->query_arrival_time;
   return 0;
}

/**
 * Client is providing meta-info (CWD, PID) to server.
 **/
static int handle_client_myrankinfo_msg(ldcs_process_data_t *procdata, int nc, ldcs_message_t *msg)
{
   int tmpdata[4];
   ldcs_message_t out_msg;
   int connid;
   char buffer_out[MAX_PATH_LEN];
   ldcs_client_t *client = procdata->client_table + nc;
   
   tmpdata[0]=nc;
   tmpdata[1]=procdata->client_counter;	/* only the current size, not the overall size */
   tmpdata[2]=procdata->md_rank;
   tmpdata[3]=procdata->md_size;
   debug_printf2("Server recvd rank info query: (%d,%d) (%d,%d)\n",
                 tmpdata[0], tmpdata[1], tmpdata[2], tmpdata[3]);

   connid = client->connid;
   out_msg.header.type = LDCS_MSG_MYRANKINFO_QUERY_ANSWER;
   out_msg.data = buffer_out;
   out_msg.header.len = sizeof(tmpdata);
   memcpy(out_msg.data, &tmpdata, out_msg.header.len);

   /* statistic */
   ldcs_send_msg(connid, &out_msg);
   procdata->server_stat.clientmsg.cnt++;
   procdata->server_stat.clientmsg.time+=(ldcs_get_time()-
                                                   client->query_arrival_time);
   return 0;
}

/**
 * Client is query'ing server for a specific file.  Could be executable, server, or open results.
 * Initializes client data structures to point to requested file.
 **/
static int handle_client_file_request(ldcs_process_data_t *procdata, int nc, ldcs_message_t *msg)
{
   char *pathname;
   char file[MAX_PATH_LEN];
   char dir[MAX_PATH_LEN];
   int is_stat, is_lstat;
   int is_loader;

   file[0] = '\0'; dir[0] = '\0';
   pathname = msg->data;
   is_stat = (msg->header.type == LDCS_MSG_STAT_QUERY) || (msg->header.type == LDCS_MSG_LSTAT_QUERY);
   is_lstat = (msg->header.type == LDCS_MSG_LSTAT_QUERY);
   is_loader = (msg->header.type == LDCS_MSG_LOADER_DATA_REQ);

   /* check to see if pathname is a local name (possibly from fstat()) and switch to global name if needed */
   char *globalname = lookup_global_name(pathname);
   if (globalname != NULL) {
      debug_printf3("Remapping request of %s --> %s\n", pathname, globalname);
      pathname = globalname;
   }
   
   parseFilenameNoAlloc(pathname, file, dir, MAX_PATH_LEN);

   /* do initial check of query, parse the filename and store info */
   assert(nc != -1);
   ldcs_client_t *client = procdata->client_table + nc;
   addCWDToDir(client->remote_pid, dir, MAX_PATH_LEN);
   reducePath(dir);

   GCC7_DISABLE_WARNING("-Wformat-truncation");
   GCC7_DISABLE_WARNING("-Wstringop-overflow");
   strncpy(client->query_filename, file, MAX_PATH_LEN+1);
   strncpy(client->query_dirname, dir, MAX_PATH_LEN+1);
   snprintf(client->query_globalpath, MAX_PATH_LEN+1, "%s/%s", 
            client->query_dirname, client->query_filename);
   GCC7_ENABLE_WARNING;
   GCC7_ENABLE_WARNING;
   
   client->query_localpath = NULL;
   
   client->query_open = 1;
   client->is_stat = is_stat;
   client->is_lstat = is_lstat;
   client->is_loader = is_loader;   
   
   debug_printf2("Server recvd query %s%s for %s.  Dir = %s, File = %s\n", 
                 is_loader ? "loader " : "",
                 is_stat ? "stat" : "exact path",
                 client->query_globalpath, client->query_dirname, client->query_filename);
   return handle_client_progress(procdata, nc);
}

/**
 * Inspect a directory request and decide whether it can be immediately
 * fulfilled, needs to be read, or be requested from the network.
 * Called from handlers
 **/
static handle_file_result_t handle_howto_directory(ldcs_process_data_t *procdata, char *dir)
{
   ldcs_cache_result_t cache_dir_result;
   int responsible;

   cache_dir_result = ldcs_cache_findDirInCache(dir);
   debug_printf2("Looked for dir in cache... %s\n",
                 ldcs_cache_result_to_str(cache_dir_result));

   if (cache_dir_result == LDCS_CACHE_DIR_PARSED_AND_EXISTS) {
      /* Directory was found */
      return FOUND_FILE;
   }
   else if (cache_dir_result == LDCS_CACHE_DIR_PARSED_AND_NOT_EXISTS) {
      return NO_FILE;
   }

   /* We need to process the directory */
   responsible = ldcs_audit_server_md_is_responsible(procdata, dir);   
   if (responsible)
      return READ_DIRECTORY;
   else
      return REQ_DIRECTORY;
}

/**
 * Inspect a file request query and decide whether it can be immediately 
 * fulfilled, needs to be read, needs a directory read, or be requested
 * from the network.
 * Called from handlers
 **/
static handle_file_result_t handle_howto_file(ldcs_process_data_t *procdata, char *pathname,
                                              char *file, char *dir,
                                              char **localpath, char **alias_to, int *errcode)
{
   int responsible = 0;
   ldcs_cache_result_t cache_filedir_result;
   handle_file_result_t dir_result;
   *alias_to = NULL;

   if (ldcs_is_a_localfile(pathname)) {
      debug_printf("Recieved request for local file %s, returning err to client\n", pathname);
      *errcode = 0;
      return FOUND_ERRCODE;
   }
   
   /* check directory + file */
   cache_filedir_result = ldcs_cache_findFileDirInCache(file, dir, localpath, errcode);
   debug_printf2("Looked for file %s in cache... %s\n",
                 pathname, ldcs_cache_result_to_str(cache_filedir_result));
                 
   if(cache_filedir_result == LDCS_CACHE_FILE_FOUND) {
      /* File was found. */
      if (*localpath) {
         /* File was stored locally.  Yeah. */
         return FOUND_FILE;
      }
      if (*errcode) {
         /* An errcode associated with reading this file was stored locally.  Less yeah. */
         return FOUND_ERRCODE;
      }

      cache_filedir_result = ldcs_cache_getAlias(file, dir, alias_to);
      if (*alias_to) {
         /* We don't have the file contents, but we've got an alias that points to something */
         return ALIAS_TO;
      }

      if (procdata->opts & OPT_STOPRELOC) {
         /* The file exists, but we are configured to not request files off network. Tell the app
            to load the original file */
         return ORIG_FILE;
      }
      /* File exists, but isn't present.  Read or request. */
      responsible = ldcs_audit_server_md_is_responsible(procdata, pathname);
      if (responsible)
         return READ_FILE;
      else
         return REQ_FILE;
   }

   /* File wasn't found.  Check state of directory */
   dir_result = handle_howto_directory(procdata, dir);
   if (dir_result == FOUND_FILE) {
      /* Directory was found, but file wasn't.  File doesn't exist. */
      return NO_FILE;
   }
   if (dir_result == NO_FILE) {
      /* Directory doesn't exist */
      return NO_DIR;
   }
   /* Do whatever is needed to load the directory */
   return dir_result;
}

/**
 * Check whether a client can make progress on any pending operations.
 **/
static int handle_client_progress(ldcs_process_data_t *procdata, int nc)
{
   handle_file_result_t result;
   int read_result, broadcast_result, client_result, errcode;
   char *alias_to;

   ldcs_client_t *client = procdata->client_table + nc;
   if ((procdata->opts & OPT_PRELOAD) && !procdata->preload_done) {
      /* Postpone client requests until preload is complete */
      debug_printf3("Postpone client requests until preload is complete\n");
      return 0;
   }
   if (!client->query_open)
      return 0;
   if (client->existance_query)
      return handle_fileexist_test(procdata, nc);
   if (client->is_stat || client->is_loader)
      return handle_client_metadata(procdata, nc);


   result = handle_howto_file(procdata, client->query_globalpath, client->query_filename,
                              client->query_dirname, &client->query_localpath,
                              &alias_to, &errcode);
   switch (result) {
      case FOUND_FILE:
         handle_client_return_numa_replication(procdata, nc);
         return handle_client_fulfilled_query(procdata, nc);
      case NO_DIR:
         return handle_client_rejected_query(procdata, nc, SPINDLE_ENODIR);
      case NO_FILE:
         return handle_client_rejected_query(procdata, nc, ENOENT);
      case FOUND_ERRCODE:
         return handle_client_rejected_query(procdata, nc, errcode);
      case READ_DIRECTORY:
         read_result = handle_read_directory(procdata, client->query_dirname);
         if (read_result == -1)
            return -1; 
         broadcast_result = handle_broadcast_dir(procdata, client->query_dirname, request_broadcast);
         client_result = handle_client_progress(procdata, nc);
         return (client_result == -1 || broadcast_result == -1) ? -1 : 0;
      case READ_FILE:
         read_result = handle_read_and_broadcast_file(procdata, client->query_globalpath, request_broadcast);
         if (read_result == -1)
            return -1;
         client_result = handle_client_progress(procdata, nc);
         return (client_result == -1 || read_result == -1) ? -1 : 0;
      case REQ_DIRECTORY:
         client_result = handle_send_query(procdata, client->query_dirname, 1);
         add_requestor(procdata->pending_requests, client->query_dirname, NODE_PEER_CLIENT);
         return client_result;
      case REQ_FILE:
         client_result = handle_send_query(procdata, client->query_globalpath, 0);
         add_requestor(procdata->pending_requests, client->query_globalpath, NODE_PEER_CLIENT);
         return client_result;
      case ORIG_FILE:
         return handle_client_originalfile_query(procdata, nc);
      case ALIAS_TO:
         handle_client_get_alias(procdata, nc, alias_to);
         return handle_client_progress(procdata, nc);
   }
   assert(0);
   return -1;
}

/**
 * Handle client file requests for all clients.
 **/
static int handle_progress(ldcs_process_data_t *procdata)
{
   int global_result = 0, nc;
   for (nc = 0; nc < procdata->client_table_used; nc++) {
      ldcs_client_t *client = procdata->client_table + nc;
      if (client->state == LDCS_CLIENT_STATUS_FREE || client->state == LDCS_CLIENT_STATUS_ACTIVE_PSEUDO)
         continue;
      int result = handle_client_progress(procdata, nc);
      if (result == -1)
         global_result = -1;
   }
   return global_result;
}

/**
 * A client requested a file that turned out to be an alias. Move the query to 
 * ask for the target of that alias.
 **/
static int handle_client_get_alias(ldcs_process_data_t *procdata, int nc, char *alias_to)
{
   ldcs_client_t *client = procdata->client_table + nc;
   debug_printf2("Alias from %s to %s. Shifting processing to target of alias\n",
                 client->query_globalpath, alias_to);
   strncpy(client->query_aliasfrom, client->query_globalpath, MAX_PATH_LEN+2);
   strncpy(client->query_globalpath, alias_to, MAX_PATH_LEN+1);
   parseFilenameNoAlloc(client->query_globalpath, client->query_filename, client->query_dirname, MAX_PATH_LEN);
   return 0;
   
}

/**
 * Read a directory contents off disk and put it into the file cache.
 **/
static int handle_read_directory(ldcs_process_data_t *procdata, char *dir)
{
   ldcs_cache_result_t cache_dir_result;
   size_t rc = 0;
   double starttime;

   /* check directory */
   cache_dir_result = ldcs_cache_findDirInCache(dir);
   if (cache_dir_result != LDCS_CACHE_DIR_NOT_PARSED) {
      return 0;
   }

   /* process directory */
   starttime = ldcs_get_time();
   debug_printf2("Reading directory: %s\n", dir );
   cache_dir_result = ldcs_cache_processDirectory(dir, &rc);
   procdata->server_stat.procdir.cnt++;
   procdata->server_stat.procdir.bytes += rc;
   procdata->server_stat.procdir.time += (ldcs_get_time() - starttime);
	
   if (cache_dir_result == LDCS_CACHE_DIR_PARSED_AND_EXISTS ||
       cache_dir_result == LDCS_CACHE_DIR_PARSED_AND_NOT_EXISTS) {
      return 0;
   }
   else {
      err_printf("Failed to process directory %s\n", dir);
      return -1;
   }
}

/**
 * Broadcast a directory contents to the specified client (if any),
 * and on the network.
 **/
static int handle_broadcast_dir(ldcs_process_data_t *procdata, char *dir, broadcast_t bcast)
{
   ldcs_message_t msg;
   char *data;
   int data_len;
   int result;
   int force_broadcast;

   debug_printf2("Sending directory result to other servers\n");      
   result = ldcs_cache_getNewEntriesForDir(dir, &data, &data_len);
   if (result == -1) {
      err_printf("Failed to get entries for directory %s\n", dir);
      return -1;
   }

   if (bcast == preload_broadcast) {
      msg.header.type = LDCS_MSG_PRELOAD_DIR;
      force_broadcast = 1;
   }
   else {
      msg.header.type = LDCS_MSG_CACHE_ENTRIES;
      force_broadcast = 0;
   }

   msg.data = data;
   msg.header.len = data_len;
   
   result = handle_send_msg_to_keys(procdata, &msg, dir, NULL, 0, force_broadcast, metadata_none);

   free(data);
   return result;
}

/**
 * Reads a directory off disk and put into the file cache.  Distribute file
 * on network if necessary.
 **/
static int handle_read_and_broadcast_dir(ldcs_process_data_t *procdata, char *dir)
{
   int result = handle_read_directory(procdata, dir);
   if (result == -1)
      return -1;
   return handle_broadcast_dir(procdata, dir, request_broadcast);
}

/**
 * We are soon going to be receiving or reading a file's contents.  This could be
 * a very large buffer.  Create an empty file on local disk for that buffer, and memory
 * map the file into our address space.  That way we can read the file without passing
 * it through the heap (which could explode our heap high-water mark).  
 * 
 * This function doesn't actually store the contents of the mapped file.  That should
 * be done by the caller, and then handle_finish_buffer_setup should be called on 
 * the region.
 **/
static void *handle_setup_file_buffer(ldcs_process_data_t *procdata, char *pathname, size_t size,
                                      int *fd, char **localname, int *already_loaded, int *replicate, is_elf_t is_elf)
{
   void *buffer;
   int result;
   char filename[MAX_PATH_LEN+1], dirname[MAX_PATH_LEN+1];
   ldcs_cache_result_t cresult;
   double starttime;
   int errcode = 0;

   debug_printf("Allocating buffer space for file %s\n", pathname);
   filename[MAX_PATH_LEN] = dirname[MAX_PATH_LEN] = '\0';   
   
   /**
    * Check if file is already in cache.
    **/
   parseFilenameNoAlloc(pathname, filename, dirname, MAX_PATH_LEN);
   cresult = ldcs_cache_findFileDirInCache(filename, dirname, localname, &errcode);
   if (cresult == LDCS_CACHE_FILE_FOUND && *localname) {
      debug_printf3("File %s was already in cache with localname %s\n", pathname, *localname);
      *already_loaded = 1;
      return NULL;
   }
   else if (cresult == LDCS_CACHE_FILE_FOUND) {
      debug_printf3("File %s was in cache, but not stored on local disk\n", pathname);
      *already_loaded = 0;
   }
   else if (cresult == LDCS_CACHE_FILE_NOT_FOUND) {
      debug_printf3("File %s wasn't in cache\n", pathname);
      ldcs_cache_addFileDir(dirname, filename);
      *already_loaded = 0;
   }
   else {
      err_printf("Unexpected return from findFileDirInCache: %d\n", (int) cresult);
      assert(0);
   }

   if (is_elf == is_elf_yes || is_elf == is_elf_unknown)
      *replicate = numa_should_replicate(procdata, pathname);
   else //is_elf == is_elf_no
      *replicate = 0;

   /**
    * Set up mapped memory for on the local disk for storing the file.
    **/
   *localname = filemngt_calc_localname(pathname, *replicate ? clt_numafile : clt_file);
   assert(*localname);
   add_global_name(pathname, *localname);
   
   starttime = ldcs_get_time();
   if (!(*replicate)) {
      result = filemngt_create_file_space(*localname, size, &buffer, fd);
      if (result == -1)
         return NULL;
   }
   else {
      buffer = numa_alloc_temporary_memory(size);
      if (buffer == NULL)
         return NULL;
      *fd = -1;
   }
   procdata->server_stat.libstore.time += (ldcs_get_time()-starttime);

   /**
    * Store the memory info in the cache
    **/
   ldcs_cache_updateBuffer(filename, dirname, *localname, buffer, size, 0);

   if (!(*replicate))
      debug_printf2("Allocated space for file %s with local file %s and mmap'd at %p\n", pathname, *localname, buffer);
   else
      debug_printf2("Allocated temporary space, pending numa copies, for file %s under pending local file %s and mmap'd at %p\n",
                    pathname, *localname, buffer);      
   

   return buffer;
}

/**
 * Finalize a buffer that a file has just been written into.
 **/
static int handle_finish_buffer_setup(ldcs_process_data_t *procdata, char *localname, 
                                      char *pathname, int *fd, 
                                      char **orig_buffer, size_t size, size_t newsize, int *replicatep, int errcode)
{
   double starttime;
   void *newbuffer = NULL, *numabuffer = NULL, *final_numabuffer;
   int i, num_nodes, result, turned_off_replication = 0;
   char numaname[MAX_PATH_LEN+1];
   char *buffer = *orig_buffer;
   int is_elf_file;
   int replicate = *replicatep;

   is_elf_file = filemngt_is_elf_file(*orig_buffer, size);
   
   starttime = ldcs_get_time();
   if (errcode) {
      debug_printf2("Releasing space at %p because read of %s returned error %d\n",
                    buffer, pathname, errcode);
      if (!replicate)
         filemngt_clear_file_space(buffer, size, *fd);
      else
         numa_free_temporary_memory(buffer, size);
      buffer = NULL;
      if (localname)
         free(localname);
      localname = NULL;
      size = 0;
   }
   else if (!replicate) {
      debug_printf2("Cleaning buffer space at %p, which is size = %lu, newsize = %lu\n", buffer,
                    (unsigned long) size, (unsigned long) newsize);
      newbuffer = filemngt_sync_file_space(buffer, *fd, localname, size, newsize);
      if (newbuffer == NULL)
         return -1;
   }
   else if (replicate && !is_elf_file) {
      //We don't actually need to replicate this non-elf file. Move it to non-replicated storage.
      debug_printf2("Turning off replication of %s for non-ELF file\n", pathname);
      int newfd = -1;
      result = filemngt_create_file_space(localname, newsize, &newbuffer, &newfd);
      if (result == -1) {
         err_printf("Failed to create space of size %lu for %s\n", newsize, localname);
         return -1;
      }
      memcpy(newbuffer, buffer, newsize);
      newbuffer = filemngt_sync_file_space(newbuffer, newfd, localname, newsize, newsize);
      if (newbuffer == NULL) {
         if (newfd != -1) close(newfd);
         return -1;
      }
      procdata->server_stat.libstore.bytes += newsize;
      if (newfd != -1) close(newfd);
      replicate = 0;
      *replicatep = 0;
      turned_off_replication = 1;
      numa_free_temporary_memory(buffer, size);      
   }
   else { //replicate && !errcode
      num_nodes = numa_num_nodes();
      debug_printf2("Replicating %s onto buffers for %d different numa domains\n", pathname, num_nodes);
      for (i = 0; i < numa_num_nodes(); i++) {
         int numafd;
         strncpy(numaname, localname, MAX_PATH_LEN);
         numa_update_local_filename(numaname, i);
         debug_printf3("Creating file for numa domain %d in %s of size %lu\n", i, numaname, newsize);
         result = filemngt_create_file_space(numaname, newsize, &numabuffer, &numafd);
         if (result == -1) {
            err_printf("Failed to create file space of size %lu for %s\n", newsize, numaname);
            if (numafd != -1) close(numafd);
            return -1;
         }
         debug_printf3("Assigning memory for %s at %p to numa node %d\n", numaname, numabuffer, i);
         result = numa_assign_memory_to_node(numabuffer, newsize, i);
         if (result == -1) {
            err_printf("Failed to associate memory region at %p with numa node %d\n", numabuffer, i);
            if (numafd != -1) close(numafd);
            return -1;
         }

         memcpy(numabuffer, buffer, newsize);
         final_numabuffer = filemngt_sync_file_space(numabuffer, numafd, numaname, newsize, newsize);
         if (final_numabuffer == NULL) {
            if (numafd != -1) close(numafd);
            return -1;
         }
         procdata->server_stat.libstore.bytes += newsize;
         
         if (i == 0) {
            //Use the 0 node replicated copy of the file for future broadcasts
            newbuffer = final_numabuffer;
         }
         if (numafd != -1)
            close(numafd);         
      }
      numa_free_temporary_memory(buffer, size);
   }
   procdata->server_stat.libstore.time += (ldcs_get_time() - starttime);      

   char filename[MAX_PATH_LEN], dirname[MAX_PATH_LEN];
   parseFilenameNoAlloc(pathname, filename, dirname, MAX_PATH_LEN);
   if (size != newsize || buffer != newbuffer || errcode) {
      /* The buffer either shrunk or moved.  Update file cache with the new information */
      ldcs_cache_updateBuffer(filename, dirname, localname, newbuffer, newsize, errcode);
      *orig_buffer = (char *) newbuffer;
   }
   if (replicate || turned_off_replication) {
      ldcs_cache_updateReplication(filename, dirname, replicate);
      *orig_buffer = (char *) newbuffer;
   }
   *fd = -1;
   return 0;
}

/**
 * If a file is an alias to another (symlink, extra slashes or dots in path)
 * then update the cache to reflect this.
 **/
static int handle_setup_alias(ldcs_process_data_t *procdata, char *pathname, char *alias_to)
{
   char filename[MAX_PATH_LEN+1], dirname[MAX_PATH_LEN+1];
   char *localname;
   int errcode;
   ldcs_cache_result_t cresult;

   filename[MAX_PATH_LEN] = dirname[MAX_PATH_LEN] = '\0';

   parseFilenameNoAlloc(pathname, filename, dirname, MAX_PATH_LEN);
   cresult = ldcs_cache_findFileDirInCache(filename, dirname, &localname, &errcode);

   assert(cresult == LDCS_CACHE_FILE_FOUND);
   debug_printf3("Updating %s in cache, which is an alias to %s\n", pathname, alias_to);
   ldcs_cache_updateAlias(filename, dirname, alias_to);
   return 0;
}

/**
 * Reads a file contents off disk and put into the file cache.  Distribute file
 * on network if necessary.
 **/
static int handle_read_and_broadcast_file(ldcs_process_data_t *procdata, char *pathname,
                                          broadcast_t bcast)
{
   double starttime;
   char *buffer = NULL, *localname = NULL;
   size_t size, newsize;
   int result, global_result = 0;
   int fd = -1;
   int errcode = 0, already_loaded;
   char alias_to_buffer[MAX_PATH_LEN+1], *alias_to = NULL;
   int replicate = 0;
   

   starttime = ldcs_get_time();
   debug_printf3("Checking if %s is an alias\n", pathname);
   result = filemngt_realpath(pathname, alias_to_buffer);   
   procdata->server_stat.metadata.time += (ldcs_get_time() - starttime);
   procdata->server_stat.metadata.cnt += 1;

   if (result == 1) {
      alias_to = alias_to_buffer;
      debug_printf2("%s is an alias for %s. Setting up\n", pathname, alias_to);

      result = handle_setup_alias(procdata, pathname, alias_to);
      if (result == -1) {
         global_result = -1;
         goto done;
      }
      goto do_broadcast;
   }

      
   /* Read file size from disk */
   starttime = ldcs_get_time();
   size = filemngt_get_file_size(pathname, &errcode);
   procdata->server_stat.metadata.time += (ldcs_get_time() - starttime);
   procdata->server_stat.metadata.cnt += 1;   
   if (size == (size_t) -1) {
      newsize = size = 0;
      goto readdone;
   }
   newsize = size;

   debug_printf2("Reading and broadcasting file %s\n", pathname);

   /* Setup buffer for file contents */
   buffer = handle_setup_file_buffer(procdata, pathname, size, &fd, &localname, &already_loaded, &replicate, is_elf_unknown);
   if (!buffer) {
      assert(!already_loaded);
      global_result = -1;
      goto done;
   }

   /* Actually read the file into the buffer */
   starttime = ldcs_get_time();

   result = filemngt_read_file(pathname, buffer, &newsize, (procdata->opts & OPT_STRIP), &errcode);
   if (result == -1) {
      global_result = -1;
      goto done;
   }

   procdata->server_stat.libread.cnt++;
   procdata->server_stat.libread.bytes += !errcode ? newsize : 0;
   procdata->server_stat.libread.time += (ldcs_get_time() - starttime);

   procdata->server_stat.libstore.cnt++;
   procdata->server_stat.libstore.bytes += !errcode && !replicate ? newsize : 0;
   procdata->server_stat.libstore.time += (ldcs_get_time() - starttime);

  readdone:   
   result = handle_finish_buffer_setup(procdata, localname, pathname, &fd, &buffer, size,
                                       newsize, &replicate, errcode);
   if (result == -1) {
      global_result = -1;
      goto done;
   }

  do_broadcast:
   if (bcast == suppress_broadcast)
      goto done;

   if (alias_to) {
      result = handle_broadcast_alias(procdata, pathname, alias_to);
   }
   else if (!errcode) {
      result = handle_broadcast_file(procdata, pathname, buffer, newsize, bcast);
   }
   else {
      result = handle_broadcast_errorcode(procdata, pathname, errcode);
   }
   if (result == -1)
      global_result = -1;

  done:
   if (fd != -1)
      close(fd);
   return global_result;
}

/**
 * Send a file's contents across the network
 **/
static int handle_broadcast_file(ldcs_process_data_t *procdata, char *pathname, char *buffer, size_t size, broadcast_t bcast)
{
   char *packet_buffer = NULL;
   size_t packet_size;
   double starttime;
   int result, global_result = 0;
   ldcs_message_t msg;
   int force_broadcast;

   result = filemngt_encode_packet(pathname, buffer, size, &packet_buffer, &packet_size);
   if (result == -1) {
      global_result = -1;
      goto done;
   }

   if (bcast == preload_broadcast) {
      msg.header.type = LDCS_MSG_PRELOAD_FILE;
      force_broadcast = 1;
   }
   else {
      msg.header.type = LDCS_MSG_FILE_DATA;
      force_broadcast = 0;
   }
   msg.header.len = packet_size;
   msg.data = packet_buffer;
   
   starttime = ldcs_get_time();
   
   result = handle_send_msg_to_keys(procdata, &msg, pathname, buffer, size, force_broadcast, metadata_none);
   if (result == -1) {
      global_result = -1;
      goto done;
   }

   procdata->server_stat.libdist.cnt++;
   procdata->server_stat.libdist.bytes += packet_size;
   procdata->server_stat.libdist.time += (ldcs_get_time()-starttime);      
   
  done:
   if (packet_buffer)
      free(packet_buffer);

   return global_result;
}

/**
 * Broadcast an error result from reading a file rather than file contents
 **/
static int handle_broadcast_errorcode(ldcs_process_data_t *procdata, char *pathname, int errcode)
{
   char *packet_buffer = NULL;
   size_t packet_size = 0;
   int result;
   int pathname_len = strlen(pathname)+1;
   int pos = 0;
   ldcs_message_t msg;
   double starttime;
   
   debug_printf2("Broadcasting errcode result %d for file %s\n", errcode, pathname);
   packet_size = sizeof(errcode);
   packet_size += sizeof(pathname_len);
   packet_size += pathname_len;
   packet_buffer = (char *) malloc(packet_size);
   memcpy(packet_buffer + pos, &errcode, sizeof(errcode));
   pos += sizeof(errcode);
   memcpy(packet_buffer + pos, &pathname_len, sizeof(pathname_len));
   pos += sizeof(pathname_len);
   strncpy(packet_buffer + pos, pathname, pathname_len);
   pos += pathname_len;
   assert(pos == packet_size);
   
   msg.header.type = LDCS_MSG_FILE_ERRCODE;
   msg.header.len = packet_size;
   msg.data = packet_buffer;

   starttime = ldcs_get_time();
   result = handle_send_msg_to_keys(procdata, &msg, pathname, NULL, 0, 0, metadata_none);
   procdata->server_stat.libdist.cnt++;
   procdata->server_stat.libdist.bytes += packet_size;
   procdata->server_stat.libdist.time += (ldcs_get_time() - starttime);      

   free(packet_buffer);
   return result;   
}

/**
 * Broadcast an alias (soft symlink) that's the result from accessing a file
 **/
static int handle_broadcast_alias(ldcs_process_data_t *procdata, char *alias_from, char *alias_to)
{
   char *packet_buffer = 0;
   size_t packet_size = 0, pos = 0, from_len, to_len;
   ldcs_message_t msg;
   int result;
   double starttime;
   
   debug_printf2("Broadcasting alias %s -> %s\n", alias_from, alias_to);
   assert(alias_from && alias_to);
   packet_size = 0;
   from_len = strlen(alias_from) + 1;
   to_len = strlen(alias_to) + 1;

   packet_size = from_len + to_len;
   packet_buffer = (char *) malloc(packet_size);
   memcpy(packet_buffer + pos, alias_from, from_len);
   pos += from_len;
   memcpy(packet_buffer + pos, alias_to, to_len);
   pos += to_len;
   assert(pos == packet_size);

   msg.header.type = LDCS_MSG_ALIAS;
   msg.header.len = packet_size;
   msg.data = packet_buffer;

   starttime = ldcs_get_time();
   result = handle_send_msg_to_keys(procdata, &msg, alias_from, NULL, 0, 0, metadata_none);
   procdata->server_stat.libdist.cnt++;
   procdata->server_stat.libdist.bytes += packet_size;
   procdata->server_stat.libdist.time += (ldcs_get_time() - starttime);      

   free(packet_buffer);
   return result;
}

static int handle_client_originalfile_query(ldcs_process_data_t *procdata, int nc)
{
   ldcs_client_t *client = procdata->client_table + nc;
   client->query_localpath = client->query_globalpath;
   debug_printf2("Server is sending original file path back: %s\n", client->query_localpath);
   return handle_client_fulfilled_query(procdata, nc);
}

/**
 * Check if the requested file is numa replicated, then update the client
 * to mark that it should send a numa replication copy of the file
 **/
static int handle_client_return_numa_replication(ldcs_process_data_t *procdata, int nc)
{
   ldcs_cache_result_t cache_filedir_result;
   ldcs_client_t *client = procdata->client_table + nc;   
   int replication;

   if (!(procdata->opts | OPT_NUMA)) {
      client->query_is_numa_replicated = 0;
      return 0;
   }
   
   cache_filedir_result = ldcs_cache_isReplicated(client->query_filename,
                              client->query_dirname, &replication);
   assert(cache_filedir_result == LDCS_CACHE_FILE_FOUND);
   client->query_is_numa_replicated = replication;
   return 0;
}

/**
 * Sends a message to a client with the local path for a sucessfully read file.
 **/
static int handle_client_fulfilled_query(ldcs_process_data_t *procdata, int nc)
{
   ldcs_message_t out_msg;
   int connid, zero = 0;
   char buffer_out[MAX_PATH_LEN+1+sizeof(int)];
   char *outfile;
   ldcs_client_t *client = procdata->client_table + nc;

   connid = client->connid;

   /* send answer only to active client not to pseudo client */
   if (client->state != LDCS_CLIENT_STATUS_ACTIVE || connid < 0)
      return 0;

   out_msg.header.type = LDCS_MSG_FILE_QUERY_ANSWER;
   out_msg.data = (void *) buffer_out;
   memcpy(out_msg.data, &zero, sizeof(int));

   outfile = out_msg.data + sizeof(int);
   strncpy(outfile, client->query_localpath, MAX_PATH_LEN);
   buffer_out[sizeof(buffer_out)-1] = '\0';
   if (client->query_is_numa_replicated) {
      debug_printf3("Updating local file %s with numa domain %d before sending to client\n", outfile, client->numa_node);
      numa_update_local_filename(outfile, client->numa_node);
   }
   
   out_msg.header.len = strlen(client->query_localpath) + 1 + sizeof(int);

   ldcs_send_msg(connid, &out_msg);

   debug_printf2("Server answering query (fulfilled): %s\n", out_msg.data);
   
   /* statistic */
   procdata->server_stat.clientmsg.cnt++;
   procdata->server_stat.clientmsg.time += ldcs_get_time() -
      client->query_arrival_time;

   handle_close_client_query(procdata, nc);
   return 0;
}

/**
 * Sends a message to a client that shows a file wasn't found.
 **/
static int handle_client_rejected_query(ldcs_process_data_t *procdata, int nc, int errcode)
{
   ldcs_message_t out_msg;
   int buffer_out;

   out_msg.header.type = LDCS_MSG_FILE_QUERY_ANSWER;
   out_msg.data = (void *) &buffer_out;
   buffer_out = errcode;
   ldcs_client_t *client = procdata->client_table + nc;
   int connid = client->connid;
   
   /* send answer only to active client not to pseudo client */
   if (client->state != LDCS_CLIENT_STATUS_ACTIVE || connid < 0)
      return 0;

   out_msg.header.len = sizeof(buffer_out);
      
   ldcs_send_msg(connid, &out_msg);

   debug_printf2("Server answering query (rejected with errcode %d)\n", errcode);
      
   /* statistic */
   procdata->server_stat.clientmsg.cnt++;
   procdata->server_stat.clientmsg.time += ldcs_get_time() - client->query_arrival_time;
   handle_close_client_query(procdata, nc);
   return 0;
}

/**
 * Set server to shutdown
 **/
static int handle_exit_broadcast(ldcs_process_data_t *procdata)
{
   ldcs_message_t out_msg;
   debug_printf("Setting up Exiting after receiving exit bcast message\n");

   out_msg.header.type = LDCS_MSG_EXIT;
   out_msg.header.len = 0;
   out_msg.data = NULL;

   spindle_broadcast(procdata, &out_msg);
   msgbundle_force_flush(procdata);

   mark_exit();
   return 0;
}

/**
 * We've received a request for a directory or file from the network.
 * Satisify it, if possible.  Otherwise mark it and forward it on.
 **/
static int handle_request(ldcs_process_data_t *procdata, node_peer_t from, ldcs_message_t *msg)
{
   int result, is_dir;
   char msg_type = msg->data[0];
   char *pathname = msg->data+1;

   debug_printf2("Got request for %s from network\n", pathname);
   if (msg_type != 'D' && msg_type != 'F') {
      err_printf("Badly formed request message with starting char '%c'\n", msg_type);
      return -1;
   }
   is_dir = (msg_type == 'D');
   if (is_dir)
      return handle_request_directory(procdata, from, pathname);
   else
      return handle_request_file(procdata, from, pathname);

   return result;
}

/**
 * We've received a request for a directory from the network.
 * Satisify it, if possible.  Otherwise mark it and forward it on.
 **/
static int handle_request_directory(ldcs_process_data_t *procdata, node_peer_t from, char *pathname)
{
   
   handle_file_result_t result = handle_howto_directory(procdata, pathname);
   int res;

   debug_printf2("Received request for directory %s from network\n", pathname);
   switch (result) {
      case READ_DIRECTORY:
         add_requestor(procdata->pending_requests, pathname, from);
         return handle_read_and_broadcast_dir(procdata, pathname);
      case REQ_DIRECTORY:
         res = handle_send_query(procdata, pathname, 1);
         add_requestor(procdata->pending_requests, pathname, from);
         return res;
      case NO_FILE:
      case FOUND_FILE:
         add_requestor(procdata->pending_requests, pathname, from);
         return handle_broadcast_dir(procdata, pathname, request_broadcast);
      default:
         err_printf("Unexpected return from handle_how_directory: %d\n", (int) result);
         assert(0);
         return -1;
   }
}

/**
 * We've receieved a request for stat data from a client.  Decide what to do with it
 **/
static handle_metadata_result_t handle_howto_metadata(ldcs_process_data_t *procdata, char *pathname, metadata_t mdtype)
{
   char *localname;
   int result, responsible;

   result = lookup_stat_cache(pathname, &localname, mdtype);
   if (result != -1)
      return REPORT_METADATA;

   responsible = ldcs_audit_server_md_is_responsible(procdata, pathname);
   if (responsible)
      return METADATA_FILE;

   if (been_requested(metadata_pending_requests(procdata, mdtype), pathname)) {
      return METADATA_IN_PROGRESS;
   }

   return REQUEST_METADATA;
}

static int handle_client_metadata(ldcs_process_data_t *procdata, int nc)
{
   handle_metadata_result_t stat_result;
   ldcs_client_t *client = procdata->client_table + nc;
   char *pathname = client->query_globalpath;
   int broadcast_result, client_result;
   metadata_t mdtype;

   if (client->is_lstat)
      mdtype = metadata_lstat;
   else if (client->is_stat)
      mdtype = metadata_stat;
   else if (client->is_loader)
      mdtype = metadata_loader;
   else
      assert(0);
      
   stat_result = handle_howto_metadata(procdata, pathname, mdtype);
   switch (stat_result) {
      case REQUEST_METADATA:
         return handle_metadata_request(procdata, pathname, mdtype, NODE_PEER_CLIENT);
      case METADATA_FILE:
         add_requestor(metadata_pending_requests(procdata, mdtype), pathname, NODE_PEER_CLIENT);
         broadcast_result = handle_metadata_and_broadcast_file(procdata, pathname, mdtype, request_broadcast);
         client_result = handle_client_metadata_result(procdata, nc, mdtype);
         if (broadcast_result == -1 || client_result == -1)
            return -1;
         return 0;
      case REPORT_METADATA:
         return handle_client_metadata_result(procdata, nc, mdtype);
      case METADATA_IN_PROGRESS:
         add_requestor(metadata_pending_requests(procdata, mdtype), pathname, NODE_PEER_CLIENT);
         return 0;
   }
   err_printf("Unexpected result from handle_howto_metadata: %d\n", (int) stat_result);
   assert(0);
   return -1;   
}

/**
 * We've received a request for a file from the network.  
 * Satisify or forward it.
 **/
static int handle_request_file(ldcs_process_data_t *procdata, node_peer_t from, char *pathname)
{
   char *localname;
   void *buffer;
   char filename[MAX_PATH_LEN], dirname[MAX_PATH_LEN];
   char *alias_to;
   size_t size;
   handle_file_result_t fresult;
   int result = 0, dir_result = 0, errcode = 0;
   
   parseFilenameNoAlloc(pathname, filename, dirname, MAX_PATH_LEN);
   fresult = handle_howto_file(procdata, pathname, filename, dirname, &localname, &alias_to, &errcode);

   debug_printf2("Received request for file %s from network\n", pathname);
   switch (fresult) {
      case FOUND_FILE:
         result = ldcs_cache_get_buffer(dirname, filename, &buffer, &size, &alias_to);
         if (result == -1) {
            err_printf("Failed to lookup %s / %s in cache\n", dirname, filename);
            return -1;
         }
         add_requestor(procdata->pending_requests, pathname, from);
         result = handle_broadcast_file(procdata, pathname, buffer, size, request_broadcast);
         return result;
      case FOUND_ERRCODE:
         add_requestor(procdata->pending_requests, pathname, from);         
         return handle_broadcast_errorcode(procdata, pathname, errcode);
      case NO_FILE:
      case NO_DIR:
      case ORIG_FILE:
         return handle_create_selfload_file(procdata, pathname);
      case READ_DIRECTORY:
         result = handle_read_and_broadcast_dir(procdata, dirname);
         if (result == -1)
            return -1;
         return handle_request_file(procdata, from, pathname);
      case READ_FILE:
         add_requestor(procdata->pending_requests, pathname, from);
         return handle_read_and_broadcast_file(procdata, pathname, request_broadcast);
      case REQ_DIRECTORY:
         dir_result = handle_send_query(procdata, dirname, 1);
         add_requestor(procdata->pending_requests, dirname, from);
         /* Fall through to next case and request file */
      case REQ_FILE:
         result = handle_send_query(procdata, pathname, 0);
         add_requestor(procdata->pending_requests, pathname, from);
         return (result == -1 || dir_result == -1) ? -1 : 0;
      case ALIAS_TO:
         add_requestor(procdata->pending_requests, pathname, from);
         return handle_broadcast_alias(procdata, pathname, alias_to);         
   }
   assert(0);
   return -1;
}

/**
 * Requests another node on the network to read a file or directory off disk
 * and forward it to us.
 **/
static int handle_send_query(ldcs_process_data_t *procdata, char *path, int is_dir)
{
   if (been_requested(procdata->pending_requests, path)) {
      debug_printf2("File %s has already been requested.  Not re-sending request\n", path);
      return 0;
   }
            
   if (is_dir)
      return handle_send_directory_query(procdata, path);
   else {
      return handle_send_file_query(procdata, path);
   }
}

/**
 * We've received request for a directory's contents. Request it from up the network.
 **/
static int handle_send_directory_query(ldcs_process_data_t *procdata, char *directory)
{
   ldcs_message_t out_msg;
   char buffer_out[MAX_PATH_LEN+1];
   int bytes_written;

   debug_printf2("Sending directory request for %s up network\n", directory);
   out_msg.header.type = LDCS_MSG_FILE_REQUEST;
   out_msg.data = buffer_out;

   bytes_written = snprintf(out_msg.data, MAX_PATH_LEN+1, "D%s", directory);
   out_msg.header.len = bytes_written+1;

   spindle_forward_query(procdata, &out_msg);
   return 0;
}

/**
 * We've received request for a files's contents. Request it from up the network.
 **/
static int handle_send_file_query(ldcs_process_data_t *procdata, char *fullpath)
{
   ldcs_message_t out_msg;
   char buffer_out[MAX_PATH_LEN+1];
   int bytes_written;

   debug_printf2("Sending file request for %s up network\n", fullpath);
   out_msg.header.type = LDCS_MSG_FILE_REQUEST;
   out_msg.data = buffer_out;

   bytes_written = snprintf(out_msg.data, MAX_PATH_LEN+1, "F%s", fullpath);
   out_msg.header.len = bytes_written+1;

   spindle_forward_query(procdata, &out_msg);
   return 0;
}

/**
 * A parent server is sending us an errcode associated with a file.  Receive it.
 **/
static int handle_file_errcode(ldcs_process_data_t *procdata, ldcs_message_t *msg, node_peer_t peer, broadcast_t bcast)
{
   char *pathname;
   int errcode, pathname_len, result, pos = 0;
   unsigned char *data;

   data = (unsigned char *) msg->data;
   errcode = *((int *) (data + pos));
   pos += sizeof(errcode);
   pathname_len = *((int *) (data + pos));
   pos += sizeof(pathname_len);
   pathname = (char *) (data + pos);
   pos += pathname_len;
   assert(pos == msg->header.len);

   debug_printf2("Received errcode result %d from read of %s\n", errcode, pathname);
   char filename[MAX_PATH_LEN], dirname[MAX_PATH_LEN];
   parseFilenameNoAlloc(pathname, filename, dirname, MAX_PATH_LEN);
   ldcs_cache_updateEntry(filename, dirname, NULL, NULL, 0, NULL, 0, errcode);

   result = handle_broadcast_errorcode(procdata, pathname, errcode);
   if (result == -1)
      return -1;

   return handle_progress(procdata);
}

/**
 * A parent server is sending us a file.  Receive it from the network
 **/
static int handle_file_recv(ldcs_process_data_t *procdata, ldcs_message_t *msg, node_peer_t peer, broadcast_t bcast)
{
   char pathname[MAX_PATH_LEN+1], *localname;
   char *buffer = NULL;
   size_t size = 0;
   int result, global_error = 0, already_loaded, fd = -1, bytes_read = 0;
   int replicate, is_elf;
   
   pathname[MAX_PATH_LEN] = '\0';

   /* If this hits, then the network layer read a entire FILE_DATA packet
      rather than just the header */
   assert(!msg->data || procdata->handling_bundle);


   /* If msg->data is empty, then we haven't read the file data off the network.  
      We'll postpone doing that until we have the memory allocated for it in a 
      mapped region of our address space.  The decode packet will just read the 
      pathname and size. */
   result = filemngt_decode_packet(peer, msg, pathname, &size, &bytes_read, &is_elf);
   if (result == -1) {
      global_error = -1;
      goto done;
   }

   debug_printf("Receiving file contents for file %s from %s\n", pathname, 
                bcast == preload_broadcast ? "preload" : "request");

   /* Setup up a memory buffer for us to read into, which is mapped to the
      local file.  Also fills in the hash table.  Does not actually read
      the file data */
   buffer = handle_setup_file_buffer(procdata, pathname, size, &fd, &localname, &already_loaded, &replicate, is_elf ? is_elf_yes : is_elf_no);
   if (!buffer) {
      if (already_loaded) {
         debug_printf("File %s was already loaded\n", pathname);
      }
      else {
         debug_printf("Problem allocating memory for buffer.  Flushing out read from the network\n");
         global_error = -1;
      }
      ldcs_audit_server_md_trash_bytes(peer, size);
      goto done;
   }

   /* Now we'll go ahead and read the file data */
   if (!msg->data) {
      result = ldcs_audit_server_md_complete_msg_read(peer, msg, buffer, size);
      if (result == -1) {
         global_error = -1;
         goto done;
      }
   }
   else {
      memcpy(buffer, ((unsigned char *) msg->data) + bytes_read, size);
   }

   /* Syncs the file contents to disk and sets local access permissions */
   result = handle_finish_buffer_setup(procdata, localname, pathname, &fd, &buffer, size, size, &replicate, 0);
   if (result == -1) {
      global_error = -1;
      goto done;
   }

   /* Notify other servers and clients of file read */
   result = handle_broadcast_file(procdata, pathname, buffer, size, bcast);
   if (result == -1) {
      global_error = -1;
   }
   result = handle_progress(procdata);
   if (result == -1) {
      global_error = -1;
   }

  done:
   if (fd != -1)
      close(fd);
   return global_error;
}

/**
 * We've received a packet with directory info.  Process it.
 **/
static int handle_directory_recv(ldcs_process_data_t *procdata, ldcs_message_t *msg, broadcast_t bcast)
{
   dirbuffer_iterator_t pos;
   char *filename, *dirname, *dir = NULL;;
   double starttime = ldcs_get_time();

   debug_printf2("New directory cache entries received from %s\n",
                 bcast == preload_broadcast ? "preload" : "request");

   /* For each directory/file in packet add it to the cache. */
   foreach_filedir(msg->data, msg->header.len, pos, filename, dirname) {
      assert(dir == NULL || dir == dirname); /* One directory per packet for now */
      dir = dirname;
      if (dirname && !filename) {
         addEmptyDirectory(dirname);
         continue;
      }
      ldcs_cache_addFileDir(dirname, filename);
   }

   handle_broadcast_dir(procdata, dir, bcast);
   
   procdata->server_stat.distdir.cnt++;
   procdata->server_stat.distdir.bytes += msg->header.len;
   procdata->server_stat.distdir.time += ldcs_get_time() - starttime;

   return handle_progress(procdata);
}

/**
 * We've received a packet with alias info. Handle it.
 **/
static int handle_alias_recv(ldcs_process_data_t *procdata, ldcs_message_t *msg, broadcast_t bcast)
{
   char *alias_from, *alias_to;
   char *data;
   size_t from_size;
   int result;

   data = msg->data;
   alias_from = data;
   from_size = strlen(alias_from);

   alias_to = data + from_size + 1;
   debug_printf2("Received alias from network %s -> %s\n", alias_from, alias_to);
   char filename[MAX_PATH_LEN], dirname[MAX_PATH_LEN];
   parseFilenameNoAlloc(alias_from, filename, dirname, MAX_PATH_LEN);
   ldcs_cache_updateAlias(filename, dirname, alias_to);

   result = handle_broadcast_alias(procdata, alias_from, alias_to);
   if (result == -1)
      return -1;

   return handle_progress(procdata);
}

/**
 * Send a message to child servers.  If in push mode we send to every child always.
 * If in pull mode only send to children who requested the file.
 **/
int handle_send_msg_to_keys(ldcs_process_data_t *procdata, ldcs_message_t *msg, char *key,
                            void *secondary_data, size_t secondary_size, int force_broadcast,
                            metadata_t mdtype)
{
   int result, global_result = 0;
   static int have_done_broadcast = 0;

   requestor_list_t pending_reqs = (mdtype == metadata_none) ? procdata->pending_requests : metadata_pending_requests(procdata, mdtype);
   requestor_list_t completed_reqs = (mdtype == metadata_none) ? procdata->completed_requests : metadata_completed_requests(procdata, mdtype);

   if (have_done_broadcast) {
      /* Test whether this file has already been broadcast to all */
      if (peer_requested(completed_reqs, key, NODE_PEER_ALL)) {
         debug_printf2("Not sending message for %s, because it's already been broadcast\n", key);
         return 0;
      }
   }

   if (procdata->dist_model == LDCS_PUSH || force_broadcast) {
      debug_printf3("Pushing message to all children\n");
      result = spindle_broadcast_noncontig(procdata, msg, secondary_data, secondary_size);
      if (result == -1)
         global_result = -1;
      have_done_broadcast = 1;
      add_requestor(completed_reqs, key, NODE_PEER_ALL);
   }
   else if (procdata->dist_model == LDCS_PULL) {
      node_peer_t *nodes = NULL;
      int nodes_size, i;

      debug_printf3("Sending messages to select children via pull model\n");
      result = get_requestors(pending_reqs, key, &nodes, &nodes_size);
      if (result == -1) {
         return 0;
      }
      debug_printf3("Sending message %s to %d nodes who requested it\n", key, nodes_size);
      for (i = 0; i < nodes_size; i++) {
         if (nodes[i] == NODE_PEER_CLIENT || nodes[i] == NODE_PEER_NULL)
            continue;
         if (peer_requested(completed_reqs, key, nodes[i])) {
            debug_printf2("Not sending message for %s to child, because it's already been sent\n", key);
            continue;
         }
         result = spindle_send_noncontig(procdata, msg, nodes[i], secondary_data, secondary_size);
         if (result == -1)
            global_result = -1;
         else
            add_requestor(completed_reqs, key, nodes[i]);
      }
   }
   else {
      assert(0);
   }

   clear_requestor(pending_reqs, key);

   return global_result;
}

/**
 * Handle a message that just arrived from a client
 **/
int handle_client_message(ldcs_process_data_t *procdata, int nc, ldcs_message_t *msg)
{
   switch (msg->header.type) {
      case LDCS_MSG_CWD:
      case LDCS_MSG_PID:
      case LDCS_MSG_LOCATION:
      case LDCS_MSG_CPU:
         return handle_client_info_msg(procdata, nc, msg);
      case LDCS_MSG_PYTHONPREFIX_REQ:
         return handle_pythonprefix_query(procdata, nc);
      case LDCS_MSG_MYRANKINFO_QUERY:
         return handle_client_myrankinfo_msg(procdata, nc, msg);
      case LDCS_MSG_FILE_QUERY:
      case LDCS_MSG_FILE_QUERY_EXACT_PATH:
      case LDCS_MSG_STAT_QUERY:
      case LDCS_MSG_LSTAT_QUERY:
      case LDCS_MSG_LOADER_DATA_REQ:
         return handle_client_file_request(procdata, nc, msg);
      case LDCS_MSG_EXISTS_QUERY:
         return handle_client_fileexist_msg(procdata, nc, msg);
      case LDCS_MSG_ORIGPATH_QUERY:
         return handle_client_origpath_msg(procdata, nc, msg);
      case LDCS_MSG_END:
         return handle_client_end(procdata, nc);
      default:
         err_printf("Received unexpected message from client %d: %d\n", nc, (int) msg->header.type);
         assert(0);
   }
   return -1;
}

/**
 * Handle a bundle of messages that just arrived from a server
 **/
static int handle_msgbundle(ldcs_process_data_t *procdata, node_peer_t peer, ldcs_message_t *msg)
{
   size_t buffer_size = msg->header.len, cur = 0;
   unsigned char *buffer_base = (unsigned char *) msg->data;
   ldcs_message_t submsg;
   int had_error = 0, result = 0;

   procdata->handling_bundle = 1;
   debug_printf2("Received message bundle of size %lu\n", buffer_size);
   while (cur < buffer_size) {
      submsg.header = *((ldcs_message_header_t *) (buffer_base + cur));
      debug_printf2("Handling message of size %lu from message bundle of size %lu at position %lu\n",
                    (unsigned long) submsg.header.len, buffer_size, cur);
      cur += sizeof(ldcs_message_header_t);
      submsg.data = (void *) (buffer_base + cur);

      result = handle_server_message(procdata, peer, &submsg);
      if (result == -1)
         had_error = -1;
      cur += submsg.header.len;
   }
   procdata->handling_bundle = 0;

   return had_error;
}

/**
 * Handle a message that just arrived from a server
 **/
int handle_server_message(ldcs_process_data_t *procdata, node_peer_t peer, ldcs_message_t *msg)
{
   switch (msg->header.type) {
      case LDCS_MSG_CACHE_ENTRIES:
         return handle_directory_recv(procdata, msg, request_broadcast);
      case LDCS_MSG_FILE_DATA:
         return handle_file_recv(procdata, msg, peer, request_broadcast);         
      case LDCS_MSG_FILE_ERRCODE:
         return handle_file_errcode(procdata, msg, peer, request_broadcast);
      case LDCS_MSG_FILE_REQUEST:
         return handle_request(procdata, peer, msg);
      case LDCS_MSG_EXIT:
         return handle_exit_broadcast(procdata);
      case LDCS_MSG_PRELOAD_FILELIST:
         return handle_preload_filelist(procdata, msg);
      case LDCS_MSG_PRELOAD_DIR:
         return handle_directory_recv(procdata, msg, preload_broadcast);
      case LDCS_MSG_PRELOAD_FILE:
         return handle_file_recv(procdata, msg, peer, preload_broadcast);
      case LDCS_MSG_PRELOAD_DONE:
         return handle_preload_done(procdata);
      case LDCS_MSG_SELFLOAD_FILE:
         return handle_recv_selfload_file(procdata, msg);
      case LDCS_MSG_STAT_NET_RESULT:
         return handle_metadata_recv(procdata, msg, metadata_stat, peer);
      case LDCS_MSG_LSTAT_NET_RESULT:
         return handle_metadata_recv(procdata, msg, metadata_lstat, peer);         
      case LDCS_MSG_STAT_NET_REQUEST:
         return handle_metadata_request_recv(procdata, msg, metadata_stat, peer);
      case LDCS_MSG_LSTAT_NET_REQUEST:
         return handle_metadata_request_recv(procdata, msg, metadata_lstat, peer);
      case LDCS_MSG_LOADER_DATA_NET_RESP:
         return handle_metadata_recv(procdata, msg, metadata_loader, peer);
      case LDCS_MSG_LOADER_DATA_NET_REQ:
         return handle_metadata_request_recv(procdata, msg, metadata_loader, peer);
      case LDCS_MSG_EXIT_READY:
         return handle_exit_ready_msg(procdata, msg);
      case LDCS_MSG_EXIT_CANCEL:
         return handle_exit_cancel_msg(procdata, msg);
      case LDCS_MSG_BUNDLE:
         return handle_msgbundle(procdata, peer, msg);
      case LDCS_MSG_ALIAS:
         return handle_alias_recv(procdata, msg, request_broadcast);
      default:
         err_printf("Received unexpected message from node: %d\n", (int) msg->header.type);
         assert(0);
   }
   return -1;
}

/**
 * Handle new client 
 **/
int handle_client_start(ldcs_process_data_t *procdata, int nc)
{
   if (procdata->sent_exit_ready) {
      return handle_send_exit_cancel(procdata);
   }
   return 0;
}

/**
 * Clean up after a client exit
 **/
int handle_client_end(ldcs_process_data_t *procdata, int nc)
{
   ldcs_client_t *client = procdata->client_table + nc;

   int connid = client->connid;
   debug_printf2("Server recvd END: closing connection\n");

   if (client->state != LDCS_CLIENT_STATUS_ACTIVE || connid < 0)
      return 0;
   
   ldcs_listen_unregister_fd(ldcs_get_fd(connid)); 
   ldcs_close_server_connection(connid);
   client->state = LDCS_CLIENT_STATUS_FREE;
   debug_printf("Closed client %d\n", nc);

   assert(procdata->clients_live > 0);
   procdata->clients_live--;
   return handle_send_exit_ready_if_done(procdata);
}

static int handle_preload_filelist(ldcs_process_data_t *procdata, ldcs_message_t *msg)
{
   int cur = 0, global_result = 0, result;
   int num_dirs, num_files, i;
   char *data = (char *) msg->data;
   char *pathname;
   
   debug_printf2("At top of handle_preload_filelist\n");

   memcpy(&num_dirs, data + cur, sizeof(int));
   cur += sizeof(int);
   
   memcpy(&num_files, data + cur, sizeof(int));
   cur += sizeof(int);

   for (i = 0; i<num_dirs; i++) {
      assert(cur < msg->header.len);
      pathname = data + cur;
      cur += strlen(pathname)+1;

      if (!ldcs_audit_server_md_is_responsible(procdata, pathname)) {
         debug_printf3("I am not responsible for preloading directory %s\n", pathname);
         continue;
      }

      debug_printf2("Preload read of directory %s\n", pathname);
      result = handle_read_directory(procdata, pathname);
      if (result == -1) {
         err_printf("Error reading directory during preload\n");
         global_result = -1;
         continue;
      }
      
      result = handle_broadcast_dir(procdata, pathname, preload_broadcast);
      if (result == -1) {
         err_printf("Error broadcasting directory during preload\n");
         global_result = -1;
         continue;
      }
   }

   for (i = 0; i<num_files; i++) {
      assert(cur < msg->header.len);
      pathname = data + cur;
      cur += strlen(pathname)+1;

      if (!ldcs_audit_server_md_is_responsible(procdata, pathname)) {
         debug_printf3("I am not responsible for preloading file %s\n", pathname);
         continue;
      }

      debug_printf2("Preload read of file %s\n", pathname);
      result = handle_read_and_broadcast_file(procdata, pathname, preload_broadcast);
      if (result == -1) {
         err_printf("Error broadcasting file data during preload\n");
         global_result = -1;
         continue;
      }
   }

   result = handle_preload_done(procdata);
   if (result == -1) {
      err_printf("Error from handle_preload_done");
      global_result = -1;
   }

   return global_result;
}

static int handle_preload_done(ldcs_process_data_t *procdata)
{
   ldcs_message_t done_msg;
   int result;

   debug_printf2("Handle preload done\n");
   procdata->preload_done = 1;

   done_msg.header.type = LDCS_MSG_PRELOAD_DONE;
   done_msg.header.len = 0;
   done_msg.data = NULL;

   result = spindle_broadcast(procdata, &done_msg);
   if (result == -1) {
      err_printf("Error broadcasting done message during preload\n");
      return -1;
   }

   return handle_progress(procdata);
}

static int handle_create_selfload_file(ldcs_process_data_t *procdata, char *filename)
{
   /* Other nodes know about a file we don't know about.  Maybe a local file? 
      Send a "read it yourself" message */
   ldcs_message_t msg;
   msg.header.type = LDCS_MSG_SELFLOAD_FILE;
   msg.header.len = strlen(filename) + 1;
   msg.data = filename;

   return handle_send_msg_to_keys(procdata, &msg, filename, NULL, 0, request_broadcast, metadata_none);
}

static int handle_recv_selfload_file(ldcs_process_data_t *procdata, ldcs_message_t *msg)
{
   char *filename = (char *) msg->data;
   int result, nc, global_result = 0, found_client = 0;

   debug_printf("Recieved notice to selfload file %s\n", filename);
   result = handle_send_msg_to_keys(procdata, msg, filename, NULL, 0, request_broadcast, metadata_none);
   if (result == -1) {
      err_printf("Could not send selfload file message\n");
      global_result = -1;
   }

   for (nc = 0; nc < procdata->client_table_used; nc++) {
      ldcs_client_t *client = procdata->client_table + nc;
      if (client->state == LDCS_CLIENT_STATUS_FREE || client->state == LDCS_CLIENT_STATUS_ACTIVE_PSEUDO)
         continue;
      if (!client->query_open)
         continue;
      if (strcmp(filename, client->query_globalpath) != 0)
         continue;

      debug_printf("We have a requesting client--self loading file %s\n", filename);
      result = handle_read_and_broadcast_file(procdata, filename, suppress_broadcast);
      if (result == -1) {
         err_printf("Could not read and broadcast local file %s\n", filename);
         global_result = -1;
      }
      found_client = 1;
   }

   if (found_client) {
      result = handle_progress(procdata);
      if (result == -1) {
         err_printf("Error from handle_progress\n");
         global_result = -1;
      }
   }

   return global_result;
}

static int handle_report_fileexist_result(ldcs_process_data_t *procdata, int nc, exist_t res)
{
   ldcs_message_t out_msg;
   uint32_t query_result;
   int result;
   ldcs_client_t *client = procdata->client_table + nc;
   int connid = client->connid;

   if (client->state != LDCS_CLIENT_STATUS_ACTIVE || connid < 0)
      return 0;

   query_result = (res == exists ? 1 : 0);

   out_msg.header.type = LDCS_MSG_EXISTS_ANSWER;
   out_msg.header.len = sizeof(query_result);
   out_msg.data = (void *) &query_result;

   result = ldcs_send_msg(connid, &out_msg);
   
   procdata->server_stat.clientmsg.cnt++;
   procdata->server_stat.clientmsg.time += ldcs_get_time() - client->query_arrival_time;

   handle_close_client_query(procdata, nc);
   
   return result;
}

static int handle_fileexist_test(ldcs_process_data_t *procdata, int nc)
{
   int result;
   int errcode;
   handle_file_result_t howto_result;
   ldcs_client_t *client;
   char *alias_to;

   client = procdata->client_table + nc;
   debug_printf2("Request to test for existance of %s\n", client->query_globalpath);

   howto_result = handle_howto_file(procdata, client->query_globalpath, client->query_filename,
                                    client->query_dirname, &client->query_localpath,
                                    &alias_to, &errcode);
   switch (howto_result) {
      case READ_FILE:
      case REQ_FILE:
      case FOUND_FILE:
      case FOUND_ERRCODE:
         return handle_report_fileexist_result(procdata, nc, exists);
      case NO_FILE:
      case NO_DIR:
         return handle_report_fileexist_result(procdata, nc, not_exists);
      case READ_DIRECTORY:
         /* We should read the directory.  After that is done, restart this operation */
         result = handle_read_and_broadcast_dir(procdata, client->query_dirname);
         if (result == -1) {
            err_printf("Error reading and broadcasting directory %s\n", client->query_dirname);
            return -1;
         }
         return handle_fileexist_test(procdata, nc);
      case REQ_DIRECTORY:
         /* We should request the directory from another node */
         result = handle_send_query(procdata, client->query_dirname, 1);
         if (result == -1) {
            err_printf("Failure sending query for directory %s\n", client->query_dirname);
            return -1;
         }
         return 0;
      case ALIAS_TO:
         /* Test for existance of alias */
         handle_client_get_alias(procdata, nc, alias_to);
         return handle_fileexist_test(procdata, nc);
      default:
         err_printf("Unexpected return %d from handle_howto_file\n", (int) howto_result);
         assert(0);
         return -1;
   }
}

static int handle_client_fileexist_msg(ldcs_process_data_t *procdata, int nc, ldcs_message_t *msg)
{
   ldcs_client_t *client;
   char *pathname;
   char file[MAX_PATH_LEN];
   char dir[MAX_PATH_LEN];

   file[0] = '\0'; dir[0] = '\0';
   pathname = msg->data;
   parseFilenameNoAlloc(pathname, file, dir, MAX_PATH_LEN);

   assert(nc != -1);
   client = procdata->client_table + nc;
   addCWDToDir(client->remote_pid, dir, MAX_PATH_LEN);
   reducePath(dir);

   GCC7_DISABLE_WARNING("-Wformat-truncation");
   strncpy(client->query_filename, file, MAX_PATH_LEN);
   strncpy(client->query_dirname, dir, MAX_PATH_LEN);

   snprintf(client->query_globalpath, MAX_PATH_LEN, "%s/%s", client->query_dirname, client->query_filename);
   GCC7_ENABLE_WARNING
      
   client->query_localpath = NULL;

   client->query_open = 1;
   client->existance_query = 1;
   
   debug_printf2("Server recvd existance query for %s.  Dir = %s, File = %s\n", 
                 client->query_globalpath, client->query_dirname, client->query_filename);
   return handle_client_progress(procdata, nc);
}

extern char *_ldcs_audit_server_tmpdir;
static int handle_client_origpath_msg(ldcs_process_data_t *procdata, int nc, ldcs_message_t *msg)
{
   ldcs_client_t *client;
   char *origpath, *newpath;
   char lookuppath[MAX_PATH_LEN+1];
   ldcs_message_t resp;
   int result;

   assert(nc != -1);
   client = procdata->client_table + nc;
   origpath = msg->data;

   lookuppath[MAX_PATH_LEN] = '\0';
   if (*origpath != '/' && *origpath != '.')
      snprintf(lookuppath, MAX_PATH_LEN, "%s/%s", _ldcs_audit_server_tmpdir, origpath);
   else
      strncpy(lookuppath, origpath, MAX_PATH_LEN);

   newpath = lookup_global_name(lookuppath);
   if (!newpath) {
      debug_printf2("Leaving origpath %s as itself\n", origpath);
      newpath = origpath;
   }
   else {
      debug_printf2("Translating origpath %s to %s\n", origpath, newpath);
   }
      

   resp.header.type = LDCS_MSG_ORIGPATH_ANSWER;
   resp.header.len = strlen(newpath)+1;
   resp.data = (void*) newpath;

   result = ldcs_send_msg(client->connid, &resp);

   procdata->server_stat.clientmsg.cnt++;
   procdata->server_stat.clientmsg.time += ldcs_get_time() - client->query_arrival_time;

   handle_close_client_query(procdata, nc);
   
   return result;
}

/**
 * Stats a file and put into file cache.  Distribute on network if necessary
 **/
static int handle_metadata_and_broadcast_file(ldcs_process_data_t *procdata, char *pathname, metadata_t mdtype, broadcast_t bcast)
{
   char *localname;
   int result;
   struct stat buf;
   ldso_info_t ldsoinfo;
   unsigned char *buffer;
   size_t buffer_size;

   debug_printf2("Stating and broadcasting file %s\n", pathname);
   
   if (mdtype == metadata_stat || mdtype == metadata_lstat) {
      result = handle_stat_file(procdata, pathname, mdtype, &localname, &buf);
      if (result == -1) {
         err_printf("Error stat'ing file %s\n", pathname);
         return -1;
      }
      buffer = (unsigned char *) &buf;
      buffer_size = sizeof(buf);
   }
   else if (mdtype == metadata_loader) {
      result = handle_read_ldso_metadata(procdata, pathname, &ldsoinfo, &localname);
      if (result == -1) {
         err_printf("Error reading ldso metadata %s\n", pathname);
         return -1;
      }
      buffer = (unsigned char *) &ldsoinfo;
      buffer_size = sizeof(ldsoinfo);
   }
   else 
      assert(0);

   /* distribute file data */
   if (bcast == suppress_broadcast) {
      return 0;
   }

   result = handle_broadcast_metadata(procdata, pathname, localname != NULL, buffer, buffer_size, mdtype);
   if (result == -1) {
      err_printf("Error broadcasting stat data for %s\n", pathname);
      return -1;
   }
   return 0;
}

static int handle_read_ldso_metadata(ldcs_process_data_t *procdata, char *pathname, ldso_info_t *ldsoinfo, char **result_file)
{
   int result;
   char *read_path;

   debug_printf2("Reading and caching ldso metadata for %s\n", pathname);
   result = lookup_stat_cache(pathname, result_file, metadata_loader);
   if (result != -1) {
      debug_printf3("LDSO metadata for %s was already cached in %s\n", pathname, *result_file);
      return 0;
   }

   read_path = pathname[0] == '$' ? pathname+1 : pathname;
   result = filemngt_get_ldso_metadata(read_path, ldsoinfo);
   if (result == -1) {
      err_printf("Error getting ldso metadata for %s\n", read_path);
      handle_cache_ldso(procdata, pathname, 0, NULL, result_file);
      return -1;
   }

   debug_printf2("Read ldso metadata.  ldso offset for %s is %ld\n", pathname, ldsoinfo->binding_offset);

   return handle_cache_ldso(procdata, pathname, 1, ldsoinfo, result_file);
}


/**
 * Stats a file on disk and puts the results into a cache
 **/
static int handle_stat_file(ldcs_process_data_t *procdata, char *pathname, metadata_t mdtype, char **localname, struct stat *buf)
{
   double starttime;
   int result, file_exists;

   /* Don't read if already cached */
   result = lookup_stat_cache(pathname, localname, mdtype);
   if (result != -1) {
      debug_printf3("File %s was already cached at %s.  Not re-stating\n",
                    pathname, *localname ? *localname : "NULL");
      return 0;
   }
   
   /* Stat file from disk */
   debug_printf3("%sstating file %s\n", (mdtype == metadata_lstat) ? "l" : "", pathname);
   starttime = ldcs_get_time();
   result = filemngt_stat(pathname, buf, (mdtype == metadata_lstat) ? 1 : 0);
   file_exists = (result != -1);
   procdata->server_stat.libread.cnt++;
   procdata->server_stat.libread.bytes += file_exists ? sizeof(struct stat) : 0;
   procdata->server_stat.libread.time += (ldcs_get_time() - starttime);
   
   return handle_cache_metadata(procdata, pathname, file_exists, mdtype, buf, localname);
}

/**
 * Puts the results of a stat into the cache
 **/
static int handle_cache_metadata(ldcs_process_data_t *procdata, char *pathname, int file_exists, metadata_t mdtype, struct stat *buf, char **localname)
{
   double starttime;
   int result;
   calc_local_t nametype;

   switch (mdtype) {
      case metadata_stat: nametype = clt_stat; break;
      case metadata_lstat: nametype = clt_lstat; break;
      case metadata_loader: nametype = clt_ldso; break;
      default: nametype = clt_unknown; break;
   }

   /* Store stat contents in cache */
   if (!file_exists) {
      debug_printf3("File %s doesn't exist based on stat\n", pathname);
      *localname = NULL;
   }
   else {
      debug_printf3("Successfully stat'd file %s\n", pathname);
      *localname = filemngt_calc_localname(pathname, nametype);
      add_global_name(pathname, *localname);
   }
   add_stat_cache(pathname, *localname, mdtype);

   if (!file_exists)
      return 0;
   
   /* Write stat contents to disk */
   starttime = ldcs_get_time();
   result = filemngt_write_stat(*localname, buf);   
   procdata->server_stat.libstore.cnt++;
   procdata->server_stat.libstore.bytes += sizeof(struct stat);
   procdata->server_stat.libstore.time += (ldcs_get_time() - starttime);
   if (result == -1) {
      err_printf("Error writing stat results for %s to %s\n", pathname, *localname);
      return -1;
   }
   return 0;
}

static int handle_cache_ldso(ldcs_process_data_t *procdata, char *pathname, int file_exists,
                             ldso_info_t *ldsoinfo, char **localname)
{
   double starttime;

   if (file_exists) {
      *localname = filemngt_calc_localname(pathname, clt_ldso);
      add_global_name(pathname, *localname);
   }
   else
      *localname = NULL;
   add_stat_cache(pathname, *localname, metadata_loader);

   if (!file_exists)
      return 0;
   
   debug_printf3("Writing ldso info to file %s\n", *localname);
   starttime = ldcs_get_time();
   filemngt_write_ldsometadata(*localname, ldsoinfo);
   procdata->server_stat.libstore.cnt++;
   procdata->server_stat.libstore.bytes += sizeof(*ldsoinfo);
   procdata->server_stat.libstore.time += (ldcs_get_time() - starttime);
   
   return 0;
}

/**
 * Distributes stat contents onto the network
 **/
static int handle_broadcast_metadata(ldcs_process_data_t *procdata, char *pathname, int file_exists, unsigned char *buf, size_t buf_size, metadata_t mdtype)
{
   char *packet_buffer = NULL;
   size_t packet_size;
   double starttime;
   int result;
   ldcs_message_t msg;
   int pathname_len = strlen(pathname) + 1;
   int pos = 0;
   struct stat *sbuf = (struct stat *) buf;
   const char *mount;
   int mount_len;

   debug_printf2("Broadcasting metadata result for %s to network (%s)\n", pathname, file_exists ? "exists" : "no exist");

   /* Devices may be different on different nodes.  Add the mount point, which can be 
      remapped to another device after recv */
   mount_len = 0;
   if (file_exists && sbuf && (mdtype == metadata_stat || mdtype == metadata_lstat)) {
      result = dev_to_mount(sbuf->st_dev, &mount);
      if (result == 0) {
         debug_printf3("Will send stat of %s with mount point %s, device %x\n", pathname, mount, (int) sbuf->st_dev);
         mount_len = strlen(mount) + 1;
      }
   }
   if (!mount_len)
      debug_printf3("Stat send of %s does not have mountpoint\n", pathname);
   
   /* Allocate and encode packet */
   packet_size = sizeof(int);
   packet_size += sizeof(int);
   packet_size += sizeof(int);   
   packet_size += pathname_len;
   packet_size += mount_len;
   packet_size += file_exists ? buf_size : 0;
   packet_buffer = (char *) malloc(packet_size);
   if (!packet_buffer) {
      err_printf("Error allocating packet\n");
      return -1;
   }

   memcpy(packet_buffer + pos, &file_exists, sizeof(int));
   pos += sizeof(int);

   memcpy(packet_buffer + pos, &pathname_len, sizeof(int));
   pos += sizeof(int);

   memcpy(packet_buffer + pos, &mount_len, sizeof(int));
   pos += sizeof(int);

   memcpy(packet_buffer + pos, pathname, pathname_len);
   pos += pathname_len;

   memcpy(packet_buffer + pos, mount, mount_len);
   pos += mount_len;

   if (file_exists) {
      memcpy(packet_buffer + pos, buf, buf_size);
      pos += buf_size;
   }
   assert(pos == packet_size);

   switch (mdtype) {
      case metadata_none: assert(0); break;
      case metadata_stat: msg.header.type = LDCS_MSG_STAT_NET_RESULT; break;
      case metadata_lstat: msg.header.type = LDCS_MSG_LSTAT_NET_RESULT; break;
      case metadata_loader: msg.header.type = LDCS_MSG_LOADER_DATA_NET_RESP; break;
   }
   msg.header.len = packet_size;
   msg.data = packet_buffer;

   /* Send packet on network */
   starttime = ldcs_get_time();
   result = handle_send_msg_to_keys(procdata, &msg, pathname, NULL, 0, 0, mdtype);
   procdata->server_stat.libdist.cnt++;
   procdata->server_stat.libdist.bytes += packet_size;
   procdata->server_stat.libdist.time += (ldcs_get_time() - starttime);      

   free(packet_buffer);
   return result;
}

/**
 * Received a stat contents packet.  Decode, cache, and broadcast.
 **/
static int handle_metadata_recv(ldcs_process_data_t *procdata, ldcs_message_t *msg, metadata_t mdtype, node_peer_t peer)
{
   int file_exists;
   char pathname[MAX_PATH_LEN+1], *localpath;
   struct stat buf;
   ldso_info_t ldsoinfo;
   int pos = 0, pathlen, result, payload_size = 0, mount_len = 0;
   char *buffer = (char *) msg->data;
   unsigned char *payload = NULL;
   char mount[MAX_PATH_LEN+1];
   mount[0] = '\0';

   /* Decode packet from network */
   memcpy(&file_exists, buffer + pos, sizeof(int));
   pos += sizeof(int);

   memcpy(&pathlen, buffer + pos, sizeof(int));
   pos += sizeof(int);
   assert(pathlen >= 0 && pathlen <= MAX_PATH_LEN);

   memcpy(&mount_len, buffer + pos, sizeof(int));
   pos += sizeof(int);
   assert(mount_len >= 0 && mount_len <= MAX_PATH_LEN);
   
   memcpy(pathname, buffer + pos, pathlen);
   pos += pathlen;

   memcpy(mount, buffer + pos, mount_len);
   pos += mount_len;

   if (file_exists) {
      payload = (unsigned char *) (buffer + pos);
      if (mdtype == metadata_stat || mdtype == metadata_lstat) {
         memcpy(&buf, buffer + pos, sizeof(buf));
         pos += sizeof(buf);
         payload_size = sizeof(buf);
      }
      else {
         memcpy(&ldsoinfo, buffer + pos, sizeof(ldsoinfo));
         pos += sizeof(ldsoinfo);
         payload_size = sizeof(ldsoinfo);
      }
   }
   assert(pos == msg->header.len);

   if (mount_len) {
      dev_t dev;
      assert((mdtype == metadata_stat || mdtype == metadata_lstat) && file_exists);
      result = mount_to_dev(mount, &dev);
      if (result == 0) {
         debug_printf3("Setting device to %x from mountpoint %s for stat recv of %s\n", (int) dev, mount, pathname);
         buf.st_dev = dev;
         payload = (unsigned char *) &buf;
      }
   }

   debug_printf2("Received packet with stat for %s (%s)\n", pathname,
                 file_exists ? "file exists" : "nonexistant file");

   /* Cache the stat results */
   if (mdtype == metadata_stat || mdtype == metadata_lstat)
      result = handle_cache_metadata(procdata, pathname, file_exists, mdtype, &buf, &localpath);
   else
      result = handle_cache_ldso(procdata, pathname, file_exists, &ldsoinfo, &localpath);
   if (result == -1) {
      err_printf("Error caching stat results for %s\n", pathname);
      return -1;
   }
 
   result = handle_broadcast_metadata(procdata, pathname, file_exists, payload, payload_size, mdtype);
   if (result == -1) {
      err_printf("Error broadcast stat results for %s\n", pathname);
      return -1;
   }

   return handle_progress(procdata);
}

/**
 * We have an answer to a metadata request (file doesn't exist or stat data).  Send
 * results to client.
 **/
static int handle_client_metadata_result(ldcs_process_data_t *procdata, int nc, metadata_t mdtype)
{
   char *localpath;
   int result, connid;
   ldcs_message_t msg;
   ldcs_client_t *client;

   assert(nc != -1);
   client = procdata->client_table + nc;
   assert(client->query_open || client->is_stat);

   connid = client->connid;
   if (client->state != LDCS_CLIENT_STATUS_ACTIVE || connid < 0)
      return 0;

   result = lookup_stat_cache(client->query_globalpath, &localpath, mdtype);
   if (result == -1) {
      debug_printf3("File %s does not yet have stat results\n", client->query_globalpath);
      return 0;
   }
   
   msg.header.type = (mdtype == metadata_stat || mdtype == metadata_lstat) ? LDCS_MSG_STAT_ANSWER : LDCS_MSG_LOADER_DATA_RESP;
   msg.header.len = localpath ? strlen(localpath)+1 : 0;
   msg.data = localpath;
   
   result = ldcs_send_msg(connid, &msg);

   procdata->server_stat.clientmsg.cnt++;
   procdata->server_stat.clientmsg.time+=(ldcs_get_time()-
                                                   client->query_arrival_time);
   handle_close_client_query(procdata, nc);
   
   return result;
}

/**
 * Send a request for a metadata up the network
 **/
static int handle_metadata_request(ldcs_process_data_t *procdata, char *pathname, metadata_t mdtype, node_peer_t from)
{
   ldcs_message_t msg;
   int pathlen;

   if (been_requested(metadata_pending_requests(procdata, mdtype), pathname)) {
      debug_printf2("Metadata %s has already been requested.  Not resending request\n", pathname);
      return 0;
   }
   debug_printf2("Request metadata of %s from up the network\n", pathname);

   add_requestor(metadata_pending_requests(procdata, mdtype), pathname, from);
   
   pathlen = strlen(pathname) + 1;

   if (mdtype == metadata_stat)
      msg.header.type = LDCS_MSG_STAT_NET_REQUEST;
   else if (mdtype == metadata_lstat)
      msg.header.type = LDCS_MSG_LSTAT_NET_REQUEST;
   else if (mdtype == metadata_loader)
      msg.header.type = LDCS_MSG_LOADER_DATA_NET_REQ;
   else
      assert(0);

   msg.header.len = pathlen;
   msg.data = pathname;

   return spindle_forward_query(procdata, &msg);
}

/**
 * We have received a metadata request from the network. Decide how to handle it.
 **/
static int handle_metadata_request_recv(ldcs_process_data_t *procdata, ldcs_message_t *msg, metadata_t mdtype, node_peer_t peer)
{
   handle_metadata_result_t stat_result;
   char *pathname;

   /* Decode packet */
   pathname = (char *) msg->data;
   assert(strlen(pathname)+1 == msg->header.len);

   /* Handle packet */
   stat_result = handle_howto_metadata(procdata, pathname, mdtype);
   switch (stat_result) {
      case REQUEST_METADATA:
         return handle_metadata_request(procdata, pathname, mdtype, peer);
      case METADATA_FILE:
         add_requestor(metadata_pending_requests(procdata, mdtype), pathname, peer);
         return handle_metadata_and_broadcast_file(procdata, pathname, mdtype, request_broadcast);
      case REPORT_METADATA:
         add_requestor(metadata_pending_requests(procdata, mdtype), pathname, peer);
         return handle_load_and_broadcast_metadata(procdata, pathname, mdtype);
      case METADATA_IN_PROGRESS:
         add_requestor(metadata_pending_requests(procdata, mdtype), pathname, peer);
         return 0;
   }
   err_printf("Unexpected result from handle_howto_metadata: %d\n", (int) stat_result);
   assert(0);
   return -1;
}

/**
 * Load a cache'd metadata result from the local file system and broadcast it.
 **/
static int handle_load_and_broadcast_metadata(ldcs_process_data_t *procdata, char *pathname, metadata_t mdtype)
{
   int result;
   char *localpath;
   struct stat buf;
   ldso_info_t ldsoinfo;
   unsigned char *buffer = NULL;
   size_t buffer_size = 0;

   debug_printf2("Loading existing metadata result for %s and broadcasting it\n", pathname);
   result = lookup_stat_cache(pathname, &localpath, mdtype);
   if (result == -1) {
      err_printf("Failure locating local file with metadata data for %s\n", pathname);
      return -1;
   }

   if (localpath) {
      if (mdtype == metadata_stat || mdtype == metadata_lstat) {
         result = filemngt_read_stat(localpath, &buf);
         buffer = (unsigned char *) &buf;
         buffer_size = sizeof(buf);
      }
      else {
         result = filemngt_read_ldsometadata(localpath, &ldsoinfo);
         buffer = (unsigned char *) &ldsoinfo;
         buffer_size = sizeof(ldsoinfo);
      }

      if (result == -1) {
         err_printf("Could not read metadata from local disk for %s (%s)\n", localpath, pathname);
         return -1;
      }
   }
   
   result = handle_broadcast_metadata(procdata, pathname, localpath != NULL, buffer, buffer_size, mdtype);
   if (result == -1) {
      err_printf("Failure broadcast stat results for %s\n", pathname);
      return -1;
   }

   return 0;
}

/**
 * If all of our clients are exited, and all of our child servers have
 * sent an exit_ready, then send an exit_ready to our parent
 **/
extern void stopprofile();
static int handle_send_exit_ready_if_done(ldcs_process_data_t *procdata)
{
   ldcs_message_t msg;
   int result;
   debug_printf2("Checking if we need to send an exit ready message\n");

   if (!procdata->clients_live) {
      stopprofile();
   }
   
   if (procdata->opts & OPT_PERSIST) {
       debug_printf2("Bottom-up exit has been disabled\n");
       return 0;
   }

   if (procdata->sent_exit_ready) {
      debug_printf2("Already sent an exit message.  Not sending another\n");
      return 0;
   }

   if (procdata->clients_live > 0) {
      debug_printf2("Still have live clients (%d).  Not exiting\n", procdata->clients_live);
      return 0;
   }

   if ((procdata->opts & OPT_BEEXIT) && (!procdata->exit_note_done)) {
      debug_printf2("BE Exit enable, and not signaled with spindleExitBE.  Not exiting\n");
      return 0;
   }
   
   int num_children = ldcs_audit_server_md_get_num_children(procdata);
   if (procdata->exit_readys_recvd < num_children) {
      debug_printf2("Not all child servers are ready to exit.\n");
      return 0;
   }
   
   msg.header.type = LDCS_MSG_EXIT_READY;
   msg.header.len = 0;
   msg.data = NULL;
   procdata->sent_exit_ready = 1;

   if (ldcs_audit_server_md_is_responsible(procdata, "")) {
      debug_printf2("Messaging FE that we're ready to exit\n");
      ldcs_audit_server_md_to_frontend(procdata, &msg);
      debug_printf("Exit globally ready.  Sending exit broadcast.\n");
      return handle_exit_broadcast(procdata);
   }
   else {
      debug_printf2("Sending exit ready message to parent\n");
      result = spindle_forward_query(procdata, &msg);
      msgbundle_force_flush(procdata);
      return result;
   }
}

/** 
 * We've recvd an exit ready message from a child.  Update our count
 * and trigger see if that makes us exit ready.
 **/
static int handle_exit_ready_msg(ldcs_process_data_t *procdata, ldcs_message_t *msg)
{
   debug_printf2("Got exit ready message\n");
   procdata->exit_readys_recvd++;
   return handle_send_exit_ready_if_done(procdata);
}

/**
 * Someone who sent us an exit ready got a new client and is now
 * canceling their exit_ready message.  We may have to cancel our own
 * exit ready if we've sent one.
 **/
static int handle_exit_cancel_msg(ldcs_process_data_t *procdata, ldcs_message_t *msg)
{
   debug_printf2("Got exit cancel\n");
   assert(procdata->exit_readys_recvd > 0);
   procdata->exit_readys_recvd--;
   
   if (!procdata->sent_exit_ready)
      return 0;

   int result = handle_send_exit_cancel(procdata);
   if (result == -1)
      return -1;

   procdata->sent_exit_ready = 0;
   return 0;
}

/**
 * Send an exit cancel to parent, if we're not the root
 **/
static int handle_send_exit_cancel(ldcs_process_data_t *procdata)
{
   ldcs_message_t msg;

   assert(procdata->sent_exit_ready);
   if (ldcs_audit_server_md_is_responsible(procdata, "")) {
      err_printf("Top of tree got exit cancel, but we've already started shutdown\n");
      return 0;
   }

   debug_printf2("Sending exit cancel to parent\n");
   msg.header.type = LDCS_MSG_EXIT_CANCEL;
   msg.header.len = 0;
   msg.data = NULL;

   return spindle_forward_query(procdata, &msg);
}

static int handle_close_client_query(ldcs_process_data_t *procdata, int nc)
{
   ldcs_client_t *client = procdata->client_table + nc;
   client->query_open = 0;
   client->existance_query = 0;
   client->query_is_numa_replicated = 0;
   client->is_stat = 0;
   client->query_globalpath[0] = client->query_filename[0] = client->query_dirname[0] = client->query_aliasfrom[0] = '\0';   
   return 0;   
}

/**
 * We got pinged via the spindleExitBE launch API call.
 **/
int exit_note_cb(int fd, int serverid, void *data)
{
   int result;
   ldcs_process_data_t *procdata = (ldcs_process_data_t *) data;
   int eresult = 0;

   debug_printf2("Setting exit_note_done from callback\n");
   procdata->exit_note_done = 1;

   result = handle_send_exit_ready_if_done(procdata);
   if (result == -1) {
      debug_printf("Failure in handle_send_exit_ready_if_done\n");
      eresult = -1;
   }

   result = handleExitNote(fd, procdata->location);
   if (result == -1) {
      debug_printf("handleExitNote failed\n");
      eresult = -1;
   }

   ldcs_listen_unregister_fd(fd);
   
   return eresult;
}
