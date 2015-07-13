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
#include "ldcs_audit_server_md.h"
#include "ldcs_cache.h"
#include "spindle_launch.h"
#include "ldcs_audit_server_requestors.h"

ldcs_process_data_t ldcs_process_data;
unsigned int opts;

int _listen_exit_loop_cb_func ( int num_fds,  void * data) {
  int rc=0;
  ldcs_process_data_t *ldcs_process_data = ( ldcs_process_data_t *) data ;

  debug_printf3("check exit condition open_conn=%d clients_connected=%d \n",
	       ldcs_process_data->client_table_used,
	       ldcs_process_data->clients_connected);
  if(ldcs_process_data->client_table_used>1) {
    ldcs_process_data->clients_connected=1;
  } else {
    if(ldcs_process_data->clients_connected == 1) {

      /* session has ended, unregister listener for new client connections */
      ldcs_listen_unregister_fd(ldcs_process_data->serverfd);

      /* unregister also md support (multi-daemon) */
      ldcs_audit_server_md_unregister_fd(ldcs_process_data);

    }
  }
  return(rc);
}

int ldcs_audit_server_network_setup(unsigned int port, unsigned int num_ports, unique_id_t unique_id, 
                                    void **packed_setup_data, int *data_size)
{
   int result;
   debug_printf2("Setting up server data structure\n");

   memset(&ldcs_process_data, 0, sizeof(ldcs_process_data));

   /* Initialize server->server network */
   ldcs_audit_server_md_init(port, num_ports, unique_id, &ldcs_process_data);

   /* Use network to broadcast configuration parameters */
   ldcs_message_t msg;
   msg.header.type = 0;
   msg.header.len = 0;
   msg.data = NULL;
   debug_printf2("Reading setup message from parent\n");
   result = ldcs_audit_server_md_recv_from_parent(&msg);
   if (result == -1) {
      err_printf("Error reading setup message from parent\n");
      return -1;
   }
   assert(msg.header.type == LDCS_MSG_SETTINGS);
   result = ldcs_audit_server_md_broadcast(&ldcs_process_data, &msg);
   if (result == -1) {
      err_printf("Error broadcast setup message to children\n");
      return -1;
   }

   *packed_setup_data = msg.data;
   *data_size = msg.header.len;

   return 0;
}

int ldcs_audit_server_process(spindle_args_t *args)
{
   int serverid, fd;

   debug_printf3("Initializing server data structures\n");
   ldcs_process_data.location = args->location;
   ldcs_process_data.number = args->number;
   ldcs_process_data.pythonprefix = args->pythonprefix;
   ldcs_process_data.md_port = args->port;
   ldcs_process_data.opts = args->opts;
   ldcs_process_data.pending_requests = new_requestor_list();
   ldcs_process_data.completed_requests = new_requestor_list();
   ldcs_process_data.pending_metadata_requests = new_requestor_list();
   ldcs_process_data.completed_metadata_requests = new_requestor_list();

   if (ldcs_process_data.opts & OPT_PULL) {
      debug_printf("Using PULL model\n");
      ldcs_process_data.dist_model = LDCS_PULL;
   }
   else if (ldcs_process_data.opts & OPT_PUSH) {
      debug_printf("Using PUSH model\n");
      ldcs_process_data.dist_model = LDCS_PUSH;
   }
   else {
      err_printf("Neither push nor pull options were set\n");
      assert(0);
   }

   _ldcs_server_stat_init(&ldcs_process_data.server_stat);

   {
      char buffer[65];
      gethostname(buffer, 65);
      ldcs_process_data.hostname = strdup(buffer);
   }
   ldcs_process_data.server_stat.hostname=ldcs_process_data.hostname;

   debug_printf3("Initializing file cache location %s\n", ldcs_process_data.location);
   ldcs_audit_server_filemngt_init(ldcs_process_data.location);

   debug_printf3("Initializing connections for clients at %s and %u\n",
                 ldcs_process_data.location, ldcs_process_data.number);
   serverid = ldcs_create_server(ldcs_process_data.location, ldcs_process_data.number);
   if (serverid == -1) {
      err_printf("Unable to setup area for client connections\n");
      return -1;
   }
   ldcs_process_data.serverid = serverid;
   fd = ldcs_get_fd(serverid);
   ldcs_process_data.serverfd = fd;
  
   ldcs_audit_server_md_register_fd(&ldcs_process_data);
  
   /* register server listen fd to listener */
   if (fd != -1)
      ldcs_listen_register_fd(fd, serverid, &_ldcs_server_CB, (void *) &ldcs_process_data);
  
   debug_printf3("Initializing cache\n");
   ldcs_cache_init();

   return 0;
}  

int ldcs_audit_server_run()
{
   /* start loop */
   debug_printf2("Entering server loop\n");
   ldcs_listen();
  
   ldcs_process_data.server_stat.listen_time= ldcs_get_time() - ldcs_process_data.server_stat.starttime;
   ldcs_process_data.server_stat.select_time=
      ldcs_process_data.server_stat.listen_time
      - ldcs_process_data.server_stat.client_cb.time
      - ldcs_process_data.server_stat.server_cb.time
      - ldcs_process_data.server_stat.md_cb.time;


   _ldcs_server_stat_print(&ldcs_process_data.server_stat);
  
   debug_printf("destroy server (%s,%d)\n", ldcs_process_data.location, ldcs_process_data.number);
   ldcs_destroy_server(ldcs_process_data.serverid);
  
   /* destroy md support (multi-daemon) */
   ldcs_audit_server_md_destroy(&ldcs_process_data);
  
   /* destroy file cache */
   if (!(ldcs_process_data.opts & OPT_NOCLEAN)) {
      ldcs_audit_server_filemngt_clean();
   }
  
   return 0;
}

 /* Statistic functions */
 void _ldcs_server_stat_init_entry ( ldcs_server_stat_entry_t *entry ) {
   entry->cnt=0.0;
   entry->bytes=0.0;
   entry->time=0.0;
 }

 int _ldcs_server_stat_init ( ldcs_server_stat_t *server_stat ) {
   int rc=0;
   server_stat->md_rank=0;
   server_stat->md_size=0;
   server_stat->md_fan_out=0;
   server_stat->num_connections=0;
   server_stat->starttime=-1;

   _ldcs_server_stat_init_entry(&server_stat->libread);
   _ldcs_server_stat_init_entry(&server_stat->libstore);
   _ldcs_server_stat_init_entry(&server_stat->libdist);
   _ldcs_server_stat_init_entry(&server_stat->procdir);
   _ldcs_server_stat_init_entry(&server_stat->distdir);
   _ldcs_server_stat_init_entry(&server_stat->client_cb);
   _ldcs_server_stat_init_entry(&server_stat->server_cb);
   _ldcs_server_stat_init_entry(&server_stat->md_cb);
   _ldcs_server_stat_init_entry(&server_stat->clientmsg);
   _ldcs_server_stat_init_entry(&server_stat->bcast);
   _ldcs_server_stat_init_entry(&server_stat->preload);

   return(rc);
 }

 /* Statistic functions */
 int _ldcs_server_stat_print ( ldcs_server_stat_t *server_stat ) {
   int rc=0;
   debug_printf("SERVER[%02d] STAT: #conn=%2d md_size=%2d md_fan_out=%2d listen_time=%8.4f select_time=%8.4f ts_first_connect=%16.6f hostname=%s\n",
	   server_stat->md_rank, 
	   server_stat->num_connections,	
	   server_stat->md_size,
	   server_stat->md_fan_out,
	   server_stat->listen_time,
	   server_stat->select_time,
	   server_stat->starttime,
	   server_stat->hostname );

#define MYFORMAT "SERVER[%02d] STAT:  %-10s, #cnt=%5d, bytes=%8.2f MB, time=%8.4f sec\n"

  debug_printf(MYFORMAT,
	  server_stat->md_rank,"libread",
	  server_stat->libread.cnt,
	  server_stat->libread.bytes/1024.0/1024.0,
	  server_stat->libread.time );

  debug_printf(MYFORMAT,
	  server_stat->md_rank,"libstore",
	  server_stat->libstore.cnt,
	  server_stat->libstore.bytes/1024.0/1024.0,
	  server_stat->libstore.time );

  debug_printf(MYFORMAT,
	  server_stat->md_rank,"libdist",
	  server_stat->libdist.cnt,
	  server_stat->libdist.bytes/1024.0/1024.0,
	  server_stat->libdist.time );

  debug_printf(MYFORMAT,
	  server_stat->md_rank,"procdir",
	  server_stat->procdir.cnt,
	  server_stat->procdir.bytes/1024.0/1024.0,
	  server_stat->procdir.time );

  debug_printf(MYFORMAT,
	  server_stat->md_rank,"distdir",
	  server_stat->distdir.cnt,
	  server_stat->distdir.bytes/1024.0/1024.0,
	  server_stat->distdir.time );

  debug_printf(MYFORMAT,
	  server_stat->md_rank,"client_cb",
	  server_stat->client_cb.cnt,
	  server_stat->client_cb.bytes/1024.0/1024.0,
	  server_stat->client_cb.time );

  debug_printf(MYFORMAT,
	  server_stat->md_rank,"server_cb",
	  server_stat->server_cb.cnt,
	  server_stat->server_cb.bytes/1024.0/1024.0,
	  server_stat->server_cb.time );

  debug_printf(MYFORMAT,
	  server_stat->md_rank,"md_cb",
	  server_stat->md_cb.cnt,
	  server_stat->md_cb.bytes/1024.0/1024.0,
	  server_stat->md_cb.time );

  debug_printf(MYFORMAT,
	  server_stat->md_rank,"cl_msg_avg",
	  server_stat->clientmsg.cnt/((server_stat->num_connections>0)?server_stat->num_connections:1),
	  server_stat->clientmsg.bytes/1024.0/1024.0,
	  server_stat->clientmsg.time/((server_stat->num_connections>0)?server_stat->num_connections:1) );

  debug_printf(MYFORMAT,
	  server_stat->md_rank,"bcast",
	  server_stat->bcast.cnt,
	  server_stat->bcast.bytes/1024.0/1024.0,
	  server_stat->bcast.time );

  debug_printf(MYFORMAT,
	  server_stat->md_rank,"preload_cb",
	  server_stat->preload.cnt,
	  server_stat->preload.bytes/1024.0/1024.0,
	  server_stat->preload.time );

  return(rc);
}

