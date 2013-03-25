#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/inotify.h>
#include <errno.h>

#include "ldcs_api.h"
#include "ldcs_api_socket.h"
#include "ldcs_api_listen.h"
#include "ldcs_audit_server_process.h"
#include "ldcs_audit_server_filemngt.h"
#include "ldcs_audit_server_md.h"
#include "ldcs_audit_server_md_msocket.h"
#include "ldcs_audit_server_md_msocket_util.h"
#include "ldcs_audit_server_md_msocket_topo.h"
#include "ldcs_cache.h"


ldcs_msocket_data_t ldcs_msocket_data;

/* callback to gather info from other server over external fabric (connecting server and frontend), e.g. COBO or MPI */
/* gets local hostname and already openened listening port and returns rank and size of each server (rank of frontend=-1)   */
/* on rank 0  (root) it should return hostlist and portlist of all servers  */
/* on rank -1 (FE)   it should return in hostlist and portlist info about root server */

int ldcs_msocket_external_fabric_CB_registered=0;
int(*ldcs_msocket_external_fabric_CB) ( char *myhostname, int myport, int *myrank, int *size, char ***hostlist, int **portlist, 
					void *data );
void* ldcs_msocket_external_fabric_CB_data=NULL;

int _ldcs_audit_server_md_msocket_init_CB ( int fd, int nc, void *data );
int _ldcs_audit_server_md_msocket_comm_CB ( int fd, int nc, void *data );
int _ldcs_audit_server_md_cobo_bcast_msg ( ldcs_process_data_t *data, ldcs_message_t *msg );

int ldcs_register_external_fabric_CB( int cb ( char*, int, int*, int*, char***, int**, void *), 
					      void *data) {
  int rc=0;
  ldcs_msocket_external_fabric_CB_registered=1;
  ldcs_msocket_external_fabric_CB=cb;
  ldcs_msocket_external_fabric_CB_data=data;
  return(rc);
}

int ldcs_audit_server_md_init ( ldcs_process_data_t *ldcs_process_data ) {
  int rc=0;

  char* ldcs_nportsstr=getenv("LDCS_NPORTS");
  char* ldcs_locmodstr=getenv("LDCS_LOCATION_MOD");

  int usedport;
  int serverfd, serverid, i;

  ldcs_msocket_data.default_num_ports = 20;
  if(ldcs_nportsstr) {
    ldcs_msocket_data.default_num_ports=atoi(ldcs_nportsstr);
  }
  ldcs_msocket_data.default_portlist = malloc(ldcs_msocket_data.default_num_ports * sizeof(int));
  if(!ldcs_msocket_data.default_portlist) _error("could not allocate portlist");
  for (i=0; i<ldcs_msocket_data.default_num_ports; i++) {
    ldcs_msocket_data.default_portlist[i] = 5000 + i;
  }
  
  /* open listening port for bootstrapping startup  */
  serverid=ldcs_audit_server_md_msocket_create_server(ldcs_process_data->location, 
						      ldcs_msocket_data.default_portlist, ldcs_msocket_data.default_num_ports, 
						      &usedport);
  if (serverid<0) {
    _error("Failed to init msocket\n");
  }
  serverfd=ldcs_get_fd_socket(serverid);

  /* init data structure */
  ldcs_msocket_data.serverfd=serverfd;
  ldcs_msocket_data.serverid=serverid;
  {
    char buffer[MAX_PATH_LEN];
    rc=gethostname(buffer, MAX_PATH_LEN);
    if(!strcmp(buffer,"zam371guest")) {
      strcpy(buffer,"localhost");
    }
    ldcs_msocket_data.hostname=strdup(buffer);
    ldcs_msocket_data.server_stat.hostname=strdup(buffer);
  }
  ldcs_msocket_data.procdata=ldcs_process_data;

  ldcs_msocket_data.hostinfo.rank=-2; /* not yet identified */
  ldcs_msocket_data.hostlist=NULL;

  /* if external fabric: distribute and/or get data about local contact info */
  if(ldcs_msocket_external_fabric_CB_registered) {
    int myerank,esize;
    int *eportlist;
    char **ehostlist; 
    ldcs_msocket_external_fabric_CB(ldcs_msocket_data.hostname, usedport, &myerank, &esize, &ehostlist, &eportlist, ldcs_msocket_external_fabric_CB_data);
    ldcs_process_data->md_rank=myerank;
    ldcs_process_data->md_size=esize;
    ldcs_msocket_data.hostinfo.rank=myerank;
    ldcs_msocket_data.hostinfo.size=esize;
    ldcs_msocket_data.hostlist=ehostlist;
    ldcs_msocket_data.portlist=eportlist;
  }

  /* register server listen fd to listener */
  ldcs_listen_register_fd(serverfd, serverid, &_ldcs_audit_server_md_msocket_server_CB, (void *) &ldcs_msocket_data);

  /* go into listen loop to enable startup CB functions, 
     the listen loop will exists after startup ofd msocket is completed */
  debug_printf3("msocket listen loop starting\n");
  ldcs_listen();
  debug_printf3("msocket listen loop finished\n");

  ldcs_listen_unregister_fd(serverfd);

  
  if(ldcs_locmodstr) {
    int ldcs_locmod=atoi(ldcs_locmodstr);
    if(ldcs_locmod>0) {
      char buffer[MAX_PATH_LEN];
      debug_printf3("multiple server per node add modifier to location mod=%d\n",ldcs_locmod);
      if(strlen(ldcs_process_data->location)+10<MAX_PATH_LEN) {
	sprintf(buffer,"%s-%02d",ldcs_process_data->location,ldcs_process_data->md_rank%ldcs_locmod);
	debug_printf3("change location to %s (locmod=%d)\n",buffer,ldcs_locmod);
	free(ldcs_process_data->location);
	ldcs_process_data->location=strdup(buffer);
      } else _error("location path too long");
    }
  } 
  
  return(rc);
}

int ldcs_audit_server_md_distribute ( ldcs_process_data_t *ldcs_process_data, ldcs_message_t* msg) {
  int rc=0;

  /* rc=_ldcs_audit_server_md_cobo_bcast_msg(ldcs_process_data, msg); */

  msg->header.mtype=LDCS_MSG_MTYPE_BCAST;
  msg->header.dest=-1;
  msg->header.source=ldcs_msocket_data.hostinfo.rank;  msg->header.from=ldcs_msocket_data.hostinfo.rank; 

  /* sent message to first host */
  debug_printf3("route bcast msg %d->%d\n",msg->header.source, msg->header.dest);
  ldcs_audit_server_md_msocket_route_msg(&ldcs_msocket_data, msg);

  return(rc);
}


int ldcs_audit_server_md_register_fd ( ldcs_process_data_t *ldcs_process_data ) {
  int rc=0;
  debug_printf3("call not used for md_msocket\n");
  return(rc);
}

int ldcs_audit_server_md_unregister_fd ( ldcs_process_data_t *ldcs_process_data ) {
  int rc=0;

  /* callbacks for md connections are further on needed for bringing down network   */
  /* therefore: instead of unregister those, a direct signal is send to listener to end */
  /*            the listener will startet again in md_destroy */
  ldcs_listen_signal_end_listen_loop();

  /* unregister all fds */
  /* ldcs_listen_unregister_fd(parent_fd); */

  return(rc);
}

int ldcs_audit_server_md_destroy ( ldcs_process_data_t *ldcs_process_data ) {
  int rc=0;
  ldcs_message_t *msg=ldcs_msg_new();

  /* TODO: implement bringdown procedure  */

#if 0
  if(ldcs_process_data->md_rank==0) { 
    int root_fd, ack=14;
    
    /* send all other server signal to stop */
    msg->header.type=LDCS_MSG_END;
    msg->header.len=0;
    _ldcs_audit_server_md_cobo_bcast_msg(ldcs_process_data, msg);

    /* send all other server signal to destroy */
    msg->header.type=LDCS_MSG_DESTROY;
    msg->header.len=0;
    _ldcs_audit_server_md_cobo_bcast_msg(ldcs_process_data, msg);
    
    /* send fe client sognal to stop (ack)  */
    cobo_get_parent_socket(&root_fd);
    
    ldcs_cobo_write_fd(root_fd, &ack, sizeof(ack));
    debug_printf3("sent FE client signal to stop %d\n",ack);

    ldcs_cobo_read_fd(root_fd, &ack, sizeof(ack));
    debug_printf3("read from FE client ack to signal to stop %d\n",ack);
  } else {

    /* receive all other server signal to destroy */
    msg->header.type=LDCS_MSG_DESTROY;
    msg->header.len=0;
    _ldcs_audit_server_md_cobo_bcast_msg(ldcs_process_data, msg);
  }
  
  if (cobo_close() != COBO_SUCCESS) {
    debug_printf3("Failed to close\n");
    printf("Failed to close\n");
    exit(1);
  }

#endif
  ldcs_msg_free(&msg);

  return(rc);
}


int ldcs_audit_server_md_is_responsible ( ldcs_process_data_t *ldcs_process_data, char *filename ) {
  int rc=0;
  
  /* this is the place to aplly the mapping function later on  */

  /* current implementation: only MD rank does file operations  */
  if(ldcs_process_data->md_rank==0) { 
    rc=1;
  } else {
    rc=0;
  }

  debug_printf3("responsible: fn=%s rank=%d  --> %s \n",filename, ldcs_process_data->md_rank, (rc)?"YES":"NO");

  return(rc);
}

int ldcs_audit_server_md_distribution_required ( ldcs_process_data_t *ldcs_process_data, char *msg ) {
  int rc=0;
  
  /* do distribution of data if more than one server available  */
  if(ldcs_process_data->md_size>1) { 
    rc=1;
  } else {
    rc=0;
  }
  return(rc);
}

int ldcs_audit_server_md_forward_query(ldcs_process_data_t *ldcs_process_data, ldcs_message_t* msg) {
  int rc=0;

  /* currently not implemented  */
  
  return(rc);
}

int _ldcs_audit_server_md_msocket_server_CB ( int infd, int serverid, void *data ) {
  int rc=0;
  ldcs_msocket_data_t *ldcs_msocket_data = (ldcs_msocket_data_t *) data ;
  int nc, fd, more_avail;
  double cb_starttime;

  cb_starttime=ldcs_get_time();

  debug_printf3("starting callback\n");

  more_avail=1;
  while(more_avail) {
    
    /* add new connection */
    nc=ldcs_audit_server_md_msocket_get_free_connection_table_entry (ldcs_msocket_data);

    ldcs_msocket_data->connection_table[nc].connid       = ldcs_open_server_connections_socket(serverid,&more_avail);
    ldcs_msocket_data->connection_table[nc].state        = LDCS_CONNECTION_STATUS_ACTIVE;
    ldcs_msocket_data->connection_table[nc].null_msg_cnt = 0;    
    ldcs_msocket_data->connection_table[nc].remote_rank  = -2; /* not yet identified */
    printf("SERVER[%02d]: open connection on host %s #c=%02d at %12.4f\n", ldcs_msocket_data->md_rank, 
	   ldcs_msocket_data->hostname, 
	   ldcs_msocket_data->connection_counter,ldcs_get_time());
    
    /* register connection fd to listener */
    fd=ldcs_get_fd_socket(ldcs_msocket_data->connection_table[nc].connid);
    ldcs_listen_register_fd(fd, nc, &_ldcs_audit_server_md_msocket_connection_CB, (void *) ldcs_msocket_data);
    ldcs_msocket_data->server_stat.num_connections++;
  }

  /* remember timestamp of connect of first connection */
  if(ldcs_msocket_data->server_stat.starttime<0) {
    ldcs_msocket_data->server_stat.starttime=cb_starttime;
  }
  ldcs_msocket_data->server_stat.server_cb.cnt++;
  ldcs_msocket_data->server_stat.server_cb.time+=(ldcs_get_time()-cb_starttime);

  debug_printf3("ending callback\n");

  return(rc);
}


int _ldcs_audit_server_md_msocket_connection_CB ( int fd, int nc, void *data ) {
  int rc=0;
  ldcs_msocket_data_t *ldcs_msocket_data = (ldcs_msocket_data_t *) data ;
  ldcs_process_data_t *ldcs_process_data = ldcs_msocket_data->procdata ;
  ldcs_message_t *msg, *out_msg;
  double starttime,cb_starttime;
#ifdef LDCSDEBUG
  double msg_time;
#endif
  int connid, do_route, do_process;
  cb_starttime=ldcs_get_time();

  connid=ldcs_msocket_data->connection_table[nc].connid;
  debug_printf3("starting callback for fd=%d nc=%d connid=%d lfd=%d rrank=%d\n",fd,nc,connid,ldcs_get_fd(connid),ldcs_msocket_data->connection_table[nc].remote_rank);
  
  /* get message  */
  starttime=ldcs_get_time();
  msg=ldcs_recv_msg_socket(connid, LDCS_READ_BLOCK);
#ifdef LDCSDEBUG
  msg_time=ldcs_get_time()-starttime;
  debug_printf3("received message on connection  nc=%d connid=%d routing: %d -> %d %s %s (%10.4fs) \n", nc, connid, 
	       msg->header.source, msg->header.dest,
	       _message_type_to_str(msg->header.type),(msg->header.mtype==LDCS_MSG_MTYPE_P2P)?"P2P":"BCAST", msg_time);
#endif

  /* check if msg dest is not addressed me or must be bcast */

  if ( msg->header.type==LDCS_MSG_MD_HOSTINFO ) {
    /* no hostinfo up to now */
    do_route=0;	do_process=1;
  } else {
    if (msg->header.mtype==LDCS_MSG_MTYPE_P2P) {
      if (msg->header.dest==ldcs_msocket_data->hostinfo.rank) {
	do_route=0;do_process=1;
      } else {
	do_route=1;do_process=0;
      }
    } else {
      /* bcast */
      do_route=1; do_process=1;
    }
  }

  debug_printf3("do_route=%d, do_process=%d \n", do_route, do_process); 

  if(do_route) {
    ldcs_audit_server_md_msocket_route_msg(ldcs_msocket_data, msg);
  }

  if(!do_process) {
    ldcs_msg_free(&msg);
    return(rc);
  }

  /* otherwise process message */
  switch(msg->header.type) {

  case LDCS_MSG_PRELOAD_FILE:
    {
      debug_printf3("MDSERVER[%02d]: new preload file name received %d (%s)\n",
		   ldcs_process_data->md_rank,msg->header.len, msg->data);
      
      rc=_ldcs_client_process_fe_preload_requests ( ldcs_process_data, msg->data );

      

      /* send bootstrap msg to client  */
      out_msg=ldcs_msg_new();
      out_msg->header.mtype=LDCS_MSG_MTYPE_P2P;
      out_msg->header.dest=-1;
      out_msg->header.source=ldcs_msocket_data->hostinfo.rank;  out_msg->header.from=ldcs_msocket_data->hostinfo.rank; 
      out_msg->header.len=0;      out_msg->alloclen=0;	out_msg->data=NULL;
      
            /* send ack */
      if(rc==1) {
	out_msg->header.type=LDCS_MSG_PRELOAD_FILE_OK;
      } else {
	out_msg->header.type=LDCS_MSG_PRELOAD_FILE_NOT_FOUND;
      }

      ldcs_send_msg_socket(connid,out_msg);

    }
    break;

  case LDCS_MSG_MD_HOSTINFO:
    {

      /* store info about local server only, action at hostlist msg  */
      ldcs_msocket_hostinfo_t *hostinfo=(ldcs_msocket_hostinfo_t *) msg->data;
      debug_printf3("HOSTINFO: received %d of %d  (%d of %d)\n", hostinfo->rank, hostinfo->size, ldcs_msocket_data->hostinfo.rank, ldcs_msocket_data->hostinfo.size); 
      if(ldcs_msocket_data->hostinfo.rank>=0) {
	if(ldcs_msocket_data->hostinfo.rank!=hostinfo->rank) {
	  _error("different rank from external fabric ...");
	}
      }
      ldcs_msocket_data->hostinfo.rank=hostinfo->rank;      ldcs_msocket_data->hostinfo.size=hostinfo->size;
      ldcs_msocket_data->hostinfo.topo=hostinfo->topo;
      ldcs_process_data->md_rank=hostinfo->rank;
      ldcs_process_data->md_size=hostinfo->size;
      
      /* info about connection */
      ldcs_msocket_data->connection_table[nc].remote_rank  = msg->header.from;
      
      /* get info about connection? */
      if(ldcs_msocket_data->connection_table[nc].remote_rank!=-2) {
	ldcs_msocket_data->connection_table[nc].remote_rank  = msg->header.from;

	/* Is this connection part of topo? */
	if ( (hostinfo->cinfo_to==ldcs_process_data->md_rank) &&
	     (hostinfo->cinfo_from>=0) ) {
	  ldcs_msocket_data->connection_table[nc].ctype=LDCS_CONNECTION_TYPE_TOPO;
	  /* pocess more info, e.g. dir ... */
	} else {
	  ldcs_msocket_data->connection_table[nc].ctype=LDCS_CONNECTION_TYPE_CONTROL;
	} 
      }
      debug_printf3("MDSERVER[%02d]: hostinfo received from %d (%d of %d, depth %d)\n",ldcs_process_data->md_rank,
		   msg->header.source, hostinfo->rank,hostinfo->size,hostinfo->depth); 
    }
    break;

  case LDCS_MSG_MD_HOSTLIST:
    {
      int rank;

      ldcs_msocket_data->hostlist = (char**) malloc(ldcs_process_data->md_size * sizeof(char*));
      if(!ldcs_msocket_data->hostlist) _error("could not allocate hostlist");
      ldcs_msocket_data->portlist = malloc(ldcs_process_data->md_size * sizeof(int));
      if(!ldcs_msocket_data->portlist) _error("could not allocate portlist");
  
      for(rank=0;rank<ldcs_process_data->md_size;rank++) {
	ldcs_msocket_data->hostlist[rank]=ldcs_audit_server_md_msocket_expand_hostname(msg->data,msg->header.len,rank);
	ldcs_msocket_data->portlist[rank]=-1;
      }
      debug_printf3("MDSERVER: hostlist received from %d (%d bytes)\n",ldcs_process_data->md_rank,msg->header.len);

    }
    break;


  case LDCS_MSG_MD_BOOTSTRAP:
    {
      if(ldcs_msocket_data->hostinfo.rank==0) {
	debug_printf3("MDSERVER: initiate bootstrap of network topology on root node\n");
	ldcs_audit_server_md_msocket_init_topo_bootstrap(ldcs_msocket_data);
      } else {
	debug_printf3("MDSERVER: run bootstrap of network topology on non-root node\n");
	ldcs_audit_server_md_msocket_run_topo_bootstrap(ldcs_msocket_data, (char *) msg->data, msg->header.len);
      }
      
    }
    break;

  case LDCS_MSG_MD_BOOTSTRAP_END:
    {
      /* ToDo: change to BARRIER over MD server */
      
      
      /* sent message to first host */
      debug_printf3("switch off initial listening for bootstrap\n");
           
      /* signal listener, that listen loop should be ended */
      ldcs_listen_signal_end_listen_loop();

      if(ldcs_msocket_data->hostinfo.rank==0) {

	out_msg=ldcs_msg_new();
	
	/* signal fe that bootstrap is ready */
	out_msg->header.type=LDCS_MSG_MD_BOOTSTRAP_END_OK;
	out_msg->header.mtype=LDCS_MSG_MTYPE_P2P;
	out_msg->header.dest=-1;
	out_msg->header.source=ldcs_msocket_data->hostinfo.rank;  out_msg->header.from=ldcs_msocket_data->hostinfo.rank; 
	out_msg->header.len=0;      out_msg->alloclen=0;	out_msg->data=NULL;

	
	/* sent message to first host */
	debug_printf3("send msg bootstrap end ok\n");
	ldcs_send_msg_socket(connid,out_msg);
	
	ldcs_msg_free(&out_msg);
	
      }
      


    }
    break;

  case LDCS_MSG_CACHE_ENTRIES:
    {
      debug_printf3("MDSERVER[%02d]: new cache entries received, insert %d bytes in local cache\n",
		   ldcs_process_data->md_rank,msg->header.len);
      /* printf("MDSERVER[%02d]: recvd NEW ENTRIES: \n", ldcs_process_data->md_rank); */
      
      ldcs_process_data->server_stat.distdir.cnt++;
      ldcs_process_data->server_stat.distdir.bytes+=msg->header.len;
      ldcs_process_data->server_stat.distdir.time+=(ldcs_get_time()-starttime);
    
      ldcs_cache_storeNewEntriesSerList(msg->data,msg->header.len);
      
      /* _ldcs_client_process_clients_requests_after_update( ldcs_process_data ); */
    }
    break;

  case LDCS_MSG_FILE_DATA:
    {
      char *filename, *dirname, *localpath;
      double starttime;
      int domangle;

      debug_printf3("MDSERVER[%02d]: new cache entries received, insert %d bytes in local cache\n",
		   ldcs_process_data->md_rank,msg->header.len);
      /* printf("MDSERVER[%02d]: recvd NEW ENTRIES: \n", ldcs_process_data->md_rank); */
      
      /* store file */
      starttime=ldcs_get_time();
      rc=ldcs_audit_server_filemngt_store_file(msg, &filename, &dirname, &localpath, &domangle);
      ldcs_process_data->server_stat.libstore.cnt++;
      ldcs_process_data->server_stat.libstore.bytes+=rc;
      ldcs_process_data->server_stat.libstore.time+=(ldcs_get_time()-starttime);
      
      /* update cache */
      ldcs_cache_updateLocalPath(filename, dirname, localpath);
      ldcs_cache_updateStatus(filename, dirname, LDCS_CACHE_OBJECT_STATUS_LOCAL_PATH);
      free(filename);  free(dirname); free(localpath);

      ldcs_process_data->server_stat.libdist.cnt++;
      ldcs_process_data->server_stat.libdist.bytes+=msg->header.len;
      ldcs_process_data->server_stat.libdist.time+=(ldcs_get_time()-starttime);
      
      _ldcs_client_process_clients_requests_after_update( ldcs_process_data );

    }
    break;

  case LDCS_MSG_END:
    {
       debug_printf3("MDSERVER[%02d]: END message received \n", ldcs_process_data->md_rank);
      
      ldcs_audit_server_md_unregister_fd ( ldcs_process_data );
      
      ldcs_process_data->md_size=1;

      _ldcs_client_process_clients_requests_after_end( ldcs_process_data );
    }
    break;
    
  default: ;
    {
      debug_printf3("MDSERVER[%03d]: recvd unknown message of type: %s len=%d data=%s ...\n", 
		   ldcs_process_data->md_rank,
		   _message_type_to_str(msg->header.type),
		   msg->header.len, msg->data );
      printf("MDSERVER[%03d]: recvd unknown message of type: %s len=%d data=%s ...\n", ldcs_process_data->md_rank,
	     _message_type_to_str(msg->header.type),
	     msg->header.len, msg->data );
      _error("wrong message");
    }
    break;
  }
  ldcs_process_data->server_stat.md_cb.cnt++;
  ldcs_process_data->server_stat.md_cb.time+=(ldcs_get_time()-cb_starttime);

  ldcs_msg_free(&msg);

  return(rc);
}



/* FRONT END functions */
int ldcs_audit_server_fe_md_open ( char **hostlist, int hostlistsize, void **data  ) {
  int rc=0;
  int num_ports   = 20;
  int num_hosts   = -1;
  int* portlist = malloc(num_ports * sizeof(int));
  int i, sersize, connid;
  char *serdata; 
  ldcs_message_t* msg;
  ldcs_message_t *in_msg;
  ldcs_msocket_hostinfo_t hostinfo;  
  int myerank, esize;
  int *eportlist;
  char **ehostlist; 
  ldcs_msocket_fe_data_t *fe_data;
  
  fe_data=(ldcs_msocket_fe_data_t *) malloc(sizeof(ldcs_msocket_fe_data_t));
  if(!fe_data) _error("could not allocate fe_data");


  /* get info about server over external fabric, if available */
  if(ldcs_msocket_external_fabric_CB_registered) {

      char buffer[MAX_PATH_LEN];
      rc=gethostname(buffer, MAX_PATH_LEN);
      if(!strcmp(buffer,"zam371guest")) {
	strcpy(buffer,"localhost");
      }
      ldcs_msocket_external_fabric_CB(buffer, -1, &myerank, &esize, &ehostlist, &eportlist, ldcs_msocket_external_fabric_CB_data);
      num_hosts=esize;
  } else {

    num_hosts=hostlistsize;
    /* build our portlist */
    for (i=0; i<num_ports; i++) {
      portlist[i] = 5000 + i;
    }

  }

  /* connect to first host */
  if(ldcs_msocket_external_fabric_CB_registered) {
    connid=ldcs_audit_server_md_msocket_connect(ehostlist[0], eportlist, 1);
    debug_printf3("after connect connid=%d (E: %s,%d)\n",connid,ehostlist[0], eportlist[0]);
  } else {
    connid=ldcs_audit_server_md_msocket_connect(hostlist[0], portlist, num_ports);
    debug_printf3("after connect connid=%d (D: %s,%d)\n",connid,hostlist[0], portlist[0]);
  }

  /* message for hostinfo */
  debug_printf3("send msg hostinfo\n");
  msg=ldcs_msg_new();

  hostinfo.rank=0;
  hostinfo.size=num_hosts;
  hostinfo.depth=0;  hostinfo.cinfo_from=hostinfo.cinfo_to=-1; /* hostinfo.cinfo_dir=-1; */ 

  msg->header.type=LDCS_MSG_MD_HOSTINFO;
  msg->header.mtype=LDCS_MSG_MTYPE_P2P;
  msg->header.source=-1;  msg->header.from=-1; 
  msg->header.dest=0;   
  msg->header.len=sizeof(hostinfo);
  msg->alloclen=sizeof(hostinfo);
  msg->data=(char *) &hostinfo;

  ldcs_send_msg_socket(connid,msg);

  if(!ldcs_msocket_external_fabric_CB_registered) {

    debug_printf3("processing hostlist of size %d\n",hostlistsize);
    ldcs_audit_server_md_msocket_serialize_hostlist(hostlist, hostlistsize, &serdata, &sersize);
    debug_printf3("serialized hostlist has size %d\n",sersize);
    
    /* message for hostlist */
    msg->header.type=LDCS_MSG_MD_HOSTLIST;
    msg->header.mtype=LDCS_MSG_MTYPE_P2P;
    msg->header.len=sersize;
    msg->alloclen=sersize;
    msg->data=serdata;

    /* sent message to first host */
    debug_printf3("send msg hostlist\n");
    ldcs_send_msg_socket(connid,msg);
  }

  /* message for start of bootstrap */
  msg->header.type=LDCS_MSG_MD_BOOTSTRAP;
  msg->header.mtype=LDCS_MSG_MTYPE_P2P;
  msg->header.len=0;
  msg->alloclen=0;
  msg->data=NULL;

  /* sent message to first host */
  debug_printf3("send msg bootstrap\n");
  ldcs_send_msg_socket(connid,msg);

  /* message for ending bootstrap */
  msg->header.type=LDCS_MSG_MD_BOOTSTRAP_END;
  msg->header.mtype=LDCS_MSG_MTYPE_BCAST;
  msg->header.source=-1;  msg->header.from=-1;   msg->header.dest=-1;   
  msg->header.len=0;
  msg->alloclen=0;
  msg->data=NULL;

  /* sent message to first host */
  debug_printf3("send msg bootstrap end\n");
  ldcs_send_msg_socket(connid,msg);


  /* receive msg from root node whether bootstrap ended  */
  in_msg=ldcs_recv_msg_socket(connid, LDCS_READ_BLOCK);
  if(in_msg->header.type==LDCS_MSG_MD_BOOTSTRAP_END_OK) {
    fprintf(stderr, "SERVERFE: bootstrap  --> successful\n");
  } else {
    fprintf(stderr, "SERVERFE: bootstrap  --> not successful\n");
  }
  ldcs_msg_free(&in_msg);

  fe_data->connid=connid;
  *data=fe_data;

  return(rc);

}

int ldcs_audit_server_fe_md_preload ( char *filename, void *data  ) {
  int rc=0;
  char buffer[MAX_PATH_LEN];
  ldcs_msocket_fe_data_t *fe_data=(ldcs_msocket_fe_data_t *) data;
  FILE * fp;
  char * line = NULL, *p;
  size_t len = 0;
  ssize_t read;
  ldcs_message_t out_msg;
  ldcs_message_t *in_msg;
  
  fprintf(stderr,"SERVERFE: open file for list of library for preload: '%s'\n",filename); 
  fp = fopen(filename, "r");  
  if (fp == NULL)  {
    fprintf(stderr,"SPINDLE FE: preloadfile not found: '%s' ... skipping \n",filename); 
    return(rc);
  }

  out_msg.header.type=LDCS_MSG_PRELOAD_FILE;
  out_msg.alloclen=MAX_PATH_LEN;  out_msg.data=buffer;

  while( (read = getline(&line, &len, fp)) >= 0) {
    
    if( (p=strchr(line,'\n')) ) *p='\0';
    if( (p=strchr(line,' ')) ) *p='\0';
    strcpy(buffer,line);

    fprintf(stderr,"SERVERFE: found library for preload: '%s'\n",line); 
    if( buffer[0]=='#') continue;

    if ( strlen(buffer)==0 ) {
      fprintf(stderr,"SERVERFE: unknown line: '%s'\n",buffer);
      continue;
    }
    out_msg.header.len=strlen(buffer)+1;
    
    debug_printf3("SERVERFE: sent preload msg(%s)  to tree root connid=%d\n",out_msg.data, fe_data->connid);
    ldcs_send_msg_socket(fe_data->connid,&out_msg);
    
    fprintf(stderr, "SERVERFE: sent preload msg(%s)  to tree root\n",out_msg.data);

    /* receive msg from root node */
    in_msg=ldcs_recv_msg_socket(fe_data->connid, LDCS_READ_BLOCK);
    if(in_msg->header.type==LDCS_MSG_PRELOAD_FILE_OK) {
      fprintf(stderr, "SERVERFE:   --> successful\n");
    } else {
      fprintf(stderr, "SERVERFE:   --> not successful\n");
    }
    ldcs_msg_free(&in_msg);
    
  }
  fclose(fp);
  if (line) free(line);

  return(rc);
}

int ldcs_audit_server_fe_md_close ( void *data  ) {
  int rc=0;

#if 0
  int root_fd, ack;

  cobo_server_get_root_socket(&root_fd);
  
  ldcs_cobo_read_fd(root_fd, &ack, sizeof(ack));
  printf("server_rsh_ldcs: got ack=%d from tree root\n",ack);
  
  ack=15;
  ldcs_cobo_write_fd(root_fd, &ack, sizeof(ack));
  printf("server_rsh_ldcs: sent ack=%d to tree root\n",ack);

  /* open and close the server */
  cobo_server_close();
#endif

  return(rc);
}
