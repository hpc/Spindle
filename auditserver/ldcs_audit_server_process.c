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
#include "ldcs_api_listen.h"
#include "ldcs_audit_server_process.h"
#include "ldcs_audit_server_filemngt.h"
#include "ldcs_audit_server_md.h"
#include "ldcs_cache.h"
#include "ldcs_api_opts.h"
#include "ldcs_audit_server_requestors.h"

ldcs_process_data_t ldcs_process_data;

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
    
int ldcs_audit_server_process (char *location, unsigned int port, int number,
                               char *pythonprefix,
                               int ready_cb_func ( void *data ), 
                               void * ready_cb_data )
{
  int rc=0;
  int serverid, fd;


  /* init internal data */
  ldcs_process_data.state=0; 	/* TDB */
  ldcs_process_data.location=strdup(location);
  ldcs_process_data.number=number;
  /* allocate inital client space */
  ldcs_process_data.client_table = (ldcs_client_t *) malloc(1 * sizeof(ldcs_client_t) );
  ldcs_process_data.client_table_size=1;

  /* initialize one pseudo client, which will handle on root node connection to fe */
  ldcs_process_data.client_table_used=1;
  ldcs_process_data.client_table[0].state=LDCS_CLIENT_STATUS_ACTIVE_PSEUDO;
  ldcs_process_data.client_table[0].connid=-1;

  if (opts & OPT_PULL)
     ldcs_process_data.dist_model = LDCS_PULL;
  else if (opts & OPT_PUSH)
     ldcs_process_data.dist_model = LDCS_PUSH;
  else {
     err_printf("Neither push nor pull options were set\n");
     assert(0);
  }

  ldcs_process_data.md_rank=0; 
  ldcs_process_data.md_size=1;
  ldcs_process_data.md_port = port;
  ldcs_process_data.md_fan_out=0; 
  ldcs_process_data.client_counter=0; /* number of clients ever connected */
  ldcs_process_data.clients_connected=0; /* will set to one once a client was connected */
  ldcs_process_data.preload_done=0;
  ldcs_process_data.pending_requests = new_requestor_list();
  ldcs_process_data.completed_requests = new_requestor_list();
  ldcs_process_data.pending_stat_requests = new_requestor_list();
  ldcs_process_data.completed_stat_requests = new_requestor_list();
  ldcs_process_data.pythonprefix = pythonprefix;

  {
    char buffer[MAX_PATH_LEN];
    
    rc=gethostname(buffer, MAX_PATH_LEN);
    ldcs_process_data.hostname=strdup(buffer);
  }

  _ldcs_server_stat_init(&ldcs_process_data.server_stat);

  /* init md support (multi-daemon) */
  debug_printf3("start md_init\n");
  ldcs_audit_server_md_init(&ldcs_process_data);
  debug_printf3("finished md_init\n");

  ldcs_process_data.server_stat.hostname=ldcs_process_data.hostname;

  {
    char buffer[MAX_PATH_LEN];
    
    rc=gethostname(buffer, MAX_PATH_LEN);
    ldcs_process_data.server_stat.hostname=strdup(buffer);
  }

   /* create or check local temporary directory */
   ldcs_audit_server_filemngt_init(ldcs_process_data.location);

   /* start server */
   debug_printf3("create server (%s,%d)\n",ldcs_process_data.location,ldcs_process_data.number);
   serverid = ldcs_create_server(ldcs_process_data.location,ldcs_process_data.number);
   if(serverid<0)  _error("in starting server");
   ldcs_process_data.serverid=serverid;
   fd=ldcs_get_fd(serverid);
   ldcs_process_data.serverfd=fd;

   /* register md support (multi-daemon) */
   ldcs_audit_server_md_register_fd(&ldcs_process_data);

   /* register server listen fd to listener */
   ldcs_listen_register_fd(fd, serverid, &_ldcs_server_CB, (void *) &ldcs_process_data);

   /* register exit loop callback */
   if (getenv("LDCS_EXIT_AFTER_SESSION")) {
     if(atoi(getenv("LDCS_EXIT_AFTER_SESSION"))==1) {
       ldcs_listen_register_exit_loop_cb(&_listen_exit_loop_cb_func, (void *) &ldcs_process_data);
     }
   }

   debug_printf3("init now cache\n");
   ldcs_cache_init();

   debug_printf3("calling ready function\n");
   ready_cb_func(ready_cb_data);


   /* start loop */
   debug_printf3("start listening\n");
   ldcs_listen();
   debug_printf3("ending listening\n");

   ldcs_process_data.server_stat.listen_time=(ldcs_get_time()-ldcs_process_data.server_stat.starttime);
   ldcs_process_data.server_stat.select_time=
     ldcs_process_data.server_stat.listen_time
     - ldcs_process_data.server_stat.client_cb.time
     - ldcs_process_data.server_stat.server_cb.time
     - ldcs_process_data.server_stat.md_cb.time;


   _ldcs_server_stat_print(&ldcs_process_data.server_stat);

   /* debug hash */
   if(0){
     char buffer[MAX_PATH_LEN];
     sprintf(buffer,"hash_dump_%02d.dat",ldcs_process_data.md_rank);
     ldcs_cache_dump(buffer);
   }

   debug_printf3("destroy server (%s,%d)\n",location,number);
   ldcs_destroy_server(serverid);

   /* destroy md support (multi-daemon) */
   ldcs_audit_server_md_destroy(&ldcs_process_data);

   /* destroy file cache */
   if (!(opts & OPT_NOCLEAN))
      ldcs_audit_server_filemngt_clean();

   return(rc);
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

