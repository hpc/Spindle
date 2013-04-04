/*
This file is part of Spindle.  For copyright information see the COPYRIGHT 
file in the top level directory, or at 
<TODO:URL>.

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

#ifndef LDCS_AUDIT_SERVER_MD_MSOCKET_H
#define LDCS_AUDIT_SERVER_MD_MSOCKET_H

#include "ldcs_audit_server_process.h"

/* connection description structure */
typedef enum {
   LDCS_TOPO_TYPE_MULTI_BINOM_TREE,
   LDCS_TOPO_TYPE_BINOM_TREE,
   LDCS_TOPO_TYPE_BIN_TREE,
   LDCS_TOPO_TYPE_RING,
   LDCS_TOPO_TYPE_UNKNOWN
} ldcs_topo_type_t;


struct ldcs_msocket_hostinfo_struct
{
  int rank;
  int size;
  int depth;
  int cinfo_from; 		/* contains info about new connection */
  int cinfo_to;
  /* int cinfo_dir;  */
  ldcs_topo_type_t topo;
};
typedef struct ldcs_msocket_hostinfo_struct ldcs_msocket_hostinfo_t;

/* connection description structure */
typedef enum {
   LDCS_CONNECTION_STATUS_ACTIVE,
   LDCS_CONNECTION_STATUS_ENDED,
   LDCS_CONNECTION_STATUS_FREE,
   LDCS_CONNECTION_STATUS_UNKNOWN
} ldcs_connection_status_t;

typedef enum {
   LDCS_CONNECTION_TYPE_CONTROL,
   LDCS_CONNECTION_TYPE_TOPO,
   LDCS_CONNECTION_TYPE_UNKNOWN
} ldcs_connection_type_t;

struct ldcs_connection_struct
{
  int                      connid;
  int                      null_msg_cnt;
  ldcs_connection_status_t state;
  int                      remote_rank;
  ldcs_connection_type_t   ctype;
};
typedef struct ldcs_connection_struct ldcs_connection_t;

struct ldcs_msocket_data_struct
{
  int serverid;			/* e.g. listening socket */
  int serverfd;
  int md_rank;
  int md_size;
  char *hostname;
  int connection_table_size;
  int connection_table_used;
  int connections_connected;
  int connection_counter;
  ldcs_connection_t* connection_table;

  ldcs_msocket_hostinfo_t hostinfo;
  ldcs_process_data_t *procdata;
  
  /* port to be tests if exact port number not is known */
  int *default_portlist;
  int default_num_ports;

  /* info about all hosts */
  char **hostlist; 		/* of length md_size */
  int   *portlist; 		/* of length md_size, -1 -> no port info, use default sequence */

  /* statistics */
  ldcs_server_stat_t server_stat;
};
typedef struct ldcs_msocket_data_struct ldcs_msocket_data_t;

struct ldcs_msocket_fe_data_struct
{
  int connid;
};
typedef struct ldcs_msocket_fe_data_struct ldcs_msocket_fe_data_t;

int _ldcs_audit_server_md_msocket_server_CB ( int fd, int nc, void *data );
int _ldcs_audit_server_md_msocket_connection_CB ( int fd, int nc, void *data );

#endif
