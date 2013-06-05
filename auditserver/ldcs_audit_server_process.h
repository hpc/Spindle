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

#ifndef LDCS_AUDIT_SERVER_PROCESS_H
#define LDCS_AUDIT_SERVER_PROCESS_H

#include "ldcs_api.h"

typedef void* requestor_list_t;

/* client description structure */
typedef enum {
   LDCS_CLIENT_STATUS_ACTIVE,
   LDCS_CLIENT_STATUS_ACTIVE_PSEUDO,
   LDCS_CLIENT_STATUS_ENDED,
   LDCS_CLIENT_STATUS_FREE,
   LDCS_CLIENT_STATUS_UNKNOWN
} ldcs_client_status_t;

typedef enum {
   LDCS_PULL,
   LDCS_PUSH
} ldcs_dist_model_t;

struct ldcs_server_stat_entry_struct
{
  int                  cnt;
  long                 bytes;
  double               time;
};
typedef struct ldcs_server_stat_entry_struct ldcs_server_stat_entry_t;

struct ldcs_server_stat_struct
{
  int                  md_rank;
  int                  md_size;
  int                  md_fan_out;
  int                  num_connections;
  double               listen_time;
  double               select_time;
  double               starttime;

  ldcs_server_stat_entry_t libread;
  ldcs_server_stat_entry_t libstore;
  ldcs_server_stat_entry_t libdist;
  ldcs_server_stat_entry_t procdir;
  ldcs_server_stat_entry_t distdir;
  ldcs_server_stat_entry_t client_cb;
  ldcs_server_stat_entry_t server_cb;
  ldcs_server_stat_entry_t md_cb;
  ldcs_server_stat_entry_t clientmsg;
  ldcs_server_stat_entry_t bcast;
  ldcs_server_stat_entry_t preload;

  char *hostname;

};
typedef struct ldcs_server_stat_struct ldcs_server_stat_t;


struct ldcs_client_struct
{
  int                  connid;
  int                  lrank;
  int                  null_msg_cnt;
  ldcs_client_status_t state;
  char                 remote_location[MAX_PATH_LEN];
  int                  remote_pid;
  char                 remote_cwd[MAX_PATH_LEN];
  int                  query_open;
  int                  existance_query;
  int                  is_stat;
  char                 query_filename[MAX_PATH_LEN];    /* hash 1st key */
  char                 query_dirname[MAX_PATH_LEN];     /* hast 2nd key */
  char                 query_globalpath[MAX_PATH_LEN];  /* path to file in global fs (dirname+filename) */
  char                 *query_localpath;                /* path to file in local temporary fs (dirname+filename) */
  double               query_arrival_time;
};
typedef struct ldcs_client_struct ldcs_client_t;

struct ldcs_process_data_struct
{
  int state;
  int client_table_size;
  int client_table_used;
  int clients_connected;
  int client_counter;
  int serverid;
  int serverfd;
  ldcs_dist_model_t dist_model;
  ldcs_client_t* client_table;
  char *location;
  char *hostname;
  char *pythonprefix;
  int number;
  int preload_done;
  requestor_list_t pending_requests;
  requestor_list_t completed_requests;
  requestor_list_t pending_stat_requests;
  requestor_list_t completed_stat_requests;

  /* multi daemon support */
  int md_rank;
  int md_size;
  int md_fan_out; 		/* number of childs */
  int md_listen_to_parent;
  unsigned int md_port;
  
  /* statistics */
  ldcs_server_stat_t server_stat;
};
typedef struct ldcs_process_data_struct ldcs_process_data_t;

int ldcs_audit_server_process (char *location, unsigned int port, int number,
                               char *pythonprefix,
                               int ready_cb_func ( void *data ), 
                               void * ready_cb_data );

int _ldcs_client_CB ( int fd, int nc, void *data );
int _ldcs_server_CB ( int infd, int serverid, void *data );

int _ldcs_client_process_clients_requests_after_end ( ldcs_process_data_t *ldcs_process_data );
int _ldcs_client_process_fe_preload_requests ( ldcs_process_data_t *ldcs_process_data, char *fn );

int _ldcs_server_stat_init ( ldcs_server_stat_t *server_stat );
int _ldcs_server_stat_print ( ldcs_server_stat_t *server_stat );

#endif
