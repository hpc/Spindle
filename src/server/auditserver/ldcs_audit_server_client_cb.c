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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/inotify.h>
#include <errno.h>
#include <assert.h>

#include "ldcs_api.h"
#include "ldcs_audit_server_process.h"
#include "ldcs_audit_server_md.h"
#include "ldcs_audit_server_filemngt.h"
#include "ldcs_audit_server_handlers.h"
#include "ldcs_api_listen.h"
#include "ldcs_cache.h" 
#define DISTCACHE 1


int _ldcs_client_dump_info ( ldcs_process_data_t *ldcs_process_data );

/* some message container */
static  char buffer_in[MAX_PATH_LEN];
/* static  char buffer_out[MAX_PATH_LEN]; */

int _ldcs_client_CB ( int fd, int id, void *data ) {
  int rc=0;
  ldcs_process_data_t *ldcs_process_data = ( ldcs_process_data_t *) data ;
  ldcs_message_t in_msg;
  double cb_starttime = ldcs_get_time();
  int connid;
  int nc;

  nc = ldcs_socket_id_to_nc(id, fd);
  if (nc == -1) {
     debug_printf("Error from ldcs_socket_id_to_nc\n");
     return -1;
  }
  if (nc == -2) {
     debug_printf3("ldcs_socket_id_to_nc returned non-fatal rc.  Dropping message\n");
     return 0;
  }
  connid = ldcs_process_data->client_table[nc].connid;

  debug_printf3("Receiving message from client %d on fd %d\n", nc, fd);
  in_msg.header.type=LDCS_MSG_UNKNOWN;
  in_msg.header.len=0;
  in_msg.data=buffer_in;

  /* get message from client */
  ldcs_recv_msg_static(connid, &in_msg, LDCS_READ_BLOCK);

  /* printf("SERVER[%03d]: received message on connection connid=%d\n", nc, connid); */
  debug_printf3("received message on connection nc=%d connid=%d\n", nc, connid);

  /* statistics */
  ldcs_process_data->client_table[nc].query_arrival_time = cb_starttime;

  rc = handle_client_message(ldcs_process_data, nc, &in_msg);
  
  debug_printf3("Finished handling client message on %d with return code %d\n", nc, rc);

  ldcs_process_data->server_stat.client_cb.cnt++;
  ldcs_process_data->server_stat.client_cb.time+=(ldcs_get_time()-cb_starttime);
	  
  return rc;
}

int _ldcs_client_dump_info ( ldcs_process_data_t *ldcs_process_data ) {
  int nc;
  int rc=0;

  for(nc=0;nc<ldcs_process_data->client_table_used;nc++) {
     debug_printf3("  CLIENT DUMP: nc = %2d, connid=%d fd=%d\n",nc,
		 ldcs_process_data->client_table[nc].connid,
		 ldcs_get_fd(ldcs_process_data->client_table[nc].connid));
  }
  return(rc);
}
