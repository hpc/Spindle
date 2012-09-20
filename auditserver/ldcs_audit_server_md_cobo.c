#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/inotify.h>
#include <errno.h>

#include "ldcs_api.h"
#include "ldcs_api_listen.h"
#include "ldcs_audit_server_process.h"
#include "ldcs_audit_server_filemngt.h"
#include "ldcs_audit_server_md.h"
#include "ldcs_cache.h"
#include "cobo.h"
#include "cobo_comm.h"

int _ldcs_audit_server_md_cobo_CB ( int fd, int nc, void *data );
int _ldcs_audit_server_md_cobo_from_fe_CB ( int fd, int nc, void *data );
int _ldcs_audit_server_md_cobo_bcast_msg ( ldcs_process_data_t *data, ldcs_message_t *msg );
int _ldcs_audit_server_md_cobo_recv_msg ( int fd, ldcs_message_t *msg );
int _ldcs_audit_server_md_cobo_send_msg ( int fd, ldcs_message_t *msg );


/* callback to gather info from other server over external fabric (connecting server and frontend), e.g. COBO or MPI */
/* gets local hostname and already openened listening port and returns rank and size of each server (rank of frontend=-1)   */
/* on rank 0 it should return also hostlist and portlist of ll servers  */
int ldcs_cobo_external_fabric_CB_registered=0;
int(*ldcs_cobo_external_fabric_CB) ( char *myhostname, int myport, int *myrank, int *size, char ***hostlist, int **portlist, 
					void *data );
void* ldcs_cobo_external_fabric_CB_data=NULL;

int _ldcs_audit_server_md_cobo_init_CB ( int fd, int nc, void *data );
int _ldcs_audit_server_md_cobo_comm_CB ( int fd, int nc, void *data );
int _ldcs_audit_server_md_cobo_bcast_msg ( ldcs_process_data_t *data, ldcs_message_t *msg );

int ldcs_register_external_fabric_CB( int cb ( char*, int, int*, int*, char***, int**, void *), 
					      void *data) {
  int rc=0;
  ldcs_cobo_external_fabric_CB_registered=1;
  ldcs_cobo_external_fabric_CB=cb;
  ldcs_cobo_external_fabric_CB_data=data;
  return(rc);
}

int ldcs_audit_server_md_init ( ldcs_process_data_t *ldcs_process_data ) {
  int rc=0;

  char* ldcs_nportsstr=getenv("LDCS_NPORTS");
  char* ldcs_locmodstr=getenv("LDCS_LOCATION_MOD");
  int* portlist = NULL;
  int num_ports = 20;
  int i, my_rank, ranks;

  if(ldcs_nportsstr) {
    num_ports=atoi(ldcs_nportsstr);
  }
  portlist = malloc(num_ports * sizeof(int));

  for (i=0; i<num_ports; i++) {
    portlist[i] = 5000 + i;
  }

  /* initialize the client (read environment variables) */
  if (cobo_open(2384932, portlist, num_ports, &my_rank, &ranks) != COBO_SUCCESS) {
    printf("Failed to init\n");
    exit(1);
  }

  /* TBD: distribute bootstrap info over cobo from frontend, eg. location, number, cache parameter, ... */
  
  debug_printf("COBO ranks: %d, Rank: %d\n", ranks, my_rank);  
  printf("COBO ranks: %d, Rank: %d\n", ranks, my_rank);  fflush(stdout);

  ldcs_process_data->server_stat.md_rank=ldcs_process_data->md_rank=my_rank;
  ldcs_process_data->server_stat.md_size=ldcs_process_data->md_size=ranks;
  ldcs_process_data->md_listen_to_parent=0;

  { int fan;
    cobo_get_num_childs(&fan);
    ldcs_process_data->server_stat.md_fan_out=ldcs_process_data->md_fan_out=fan;
  }

  if(ldcs_locmodstr) {
    int ldcs_locmod=atoi(ldcs_locmodstr);
    if(ldcs_locmod>0) {
      char buffer[MAX_PATH_LEN];
      debug_printf("multiple server per node add modifier to location mod=%d\n",ldcs_locmod);
      if(strlen(ldcs_process_data->location)+10<MAX_PATH_LEN) {
	sprintf(buffer,"%s-%02d",ldcs_process_data->location,my_rank%ldcs_locmod);
	debug_printf("change location to %s (locmod=%d)\n",buffer,ldcs_locmod);
	free(ldcs_process_data->location);
	ldcs_process_data->location=strdup(buffer);
      } else _error("location path too long");
    }
  } 
  free(portlist);


  cobo_barrier();

  /* send ack about being ready */
  if(ldcs_process_data->md_rank==0) { 
    int root_fd, ack=13;

    /* send fe client signal to stop (ack)  */
    cobo_get_parent_socket(&root_fd);
    
    ldcs_cobo_write_fd(root_fd, &ack, sizeof(ack));
    debug_printf("sent FE client signal that server are ready %d\n",ack);

  }

  
  return(rc);
}

int ldcs_audit_server_md_distribute ( ldcs_process_data_t *ldcs_process_data, ldcs_message_t* msg) {
  int rc=0;

  rc=_ldcs_audit_server_md_cobo_bcast_msg(ldcs_process_data, msg);

  return(rc);
}


int ldcs_audit_server_md_register_fd ( ldcs_process_data_t *ldcs_process_data ) {
  int rc=0;
  int parent_fd;
  if(cobo_get_parent_socket(&parent_fd)!=COBO_SUCCESS) {
    _error("cobo internal error (parent socket)");
  }
  debug_printf("got fd for cobo connection to parent %d and register it\n",parent_fd);
  if(ldcs_process_data->md_rank>0) {
    ldcs_listen_register_fd(parent_fd, 0, &_ldcs_audit_server_md_cobo_CB, (void *) ldcs_process_data);
  } else {
    ldcs_listen_register_fd(parent_fd, 0, &_ldcs_audit_server_md_cobo_from_fe_CB, (void *) ldcs_process_data);
  }
  ldcs_process_data->md_listen_to_parent=1;
  return(rc);
}

int ldcs_audit_server_md_unregister_fd ( ldcs_process_data_t *ldcs_process_data ) {
  int rc=0;
  int parent_fd;
  if(ldcs_process_data->md_listen_to_parent) {
    if(cobo_get_parent_socket(&parent_fd)!=COBO_SUCCESS) {
      _error("cobo internal error (parent socket)");
    }
    ldcs_process_data->md_listen_to_parent=0;
    ldcs_listen_unregister_fd(parent_fd);
  }

  return(rc);
}

int ldcs_audit_server_md_destroy ( ldcs_process_data_t *ldcs_process_data ) {
  int rc=0;
  ldcs_message_t *msg=ldcs_msg_new();

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
    
    /* send fe client signal to stop (ack)  */
    cobo_get_parent_socket(&root_fd);
    
    ldcs_cobo_write_fd(root_fd, &ack, sizeof(ack));
    debug_printf("sent FE client signal to stop %d\n",ack);

    ldcs_cobo_read_fd(root_fd, &ack, sizeof(ack));
    debug_printf("read from FE client ack to signal to stop %d\n",ack);
  } else {

    /* receive all other server signal to destroy */
    msg->header.type=LDCS_MSG_DESTROY;
    msg->header.len=0;
    _ldcs_audit_server_md_cobo_bcast_msg(ldcs_process_data, msg);
  }
  
  if (cobo_close() != COBO_SUCCESS) {
    debug_printf("Failed to close\n");
    printf("Failed to close\n");
    exit(1);
  }
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

  debug_printf("responsible: fn=%s rank=%d  --> %s \n",filename, ldcs_process_data->md_rank, (rc)?"YES":"NO");

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


int _ldcs_audit_server_md_cobo_CB ( int fd, int nc, void *data ) {
  int rc=0;
  ldcs_process_data_t *ldcs_process_data = ( ldcs_process_data_t *) data ;
  ldcs_message_t *msg=ldcs_msg_new();
  double starttime,cb_starttime;
  cb_starttime=ldcs_get_time();
  
  /* receive msg from cobo network */
  starttime=ldcs_get_time();
  _ldcs_audit_server_md_cobo_bcast_msg(ldcs_process_data, msg);
  /* msg_time=ldcs_get_time()-starttime; */
  
  /* process message */
  switch(msg->header.type) {
  case LDCS_MSG_CACHE_ENTRIES:
    {
      debug_printf("MDSERVER[%02d]: new cache entries received, insert %d bytes in local cache\n",
		   ldcs_process_data->md_rank,msg->header.len);
      /* printf("MDSERVER[%02d]: recvd NEW ENTRIES: \n", ldcs_process_data->md_rank); */
      
      ldcs_process_data->server_stat.distdir.cnt++;
      ldcs_process_data->server_stat.distdir.bytes+=msg->header.len;
      ldcs_process_data->server_stat.distdir.time+=(ldcs_get_time()-starttime);
    
      ldcs_cache_storeNewEntriesSerList(msg->data,msg->header.len);
      
      _ldcs_client_process_clients_requests_after_update( ldcs_process_data );
    }
    break;

  case LDCS_MSG_FILE_DATA:
    {
      char *filename, *dirname, *localpath;
      double starttime;
      int domangle;

      debug_printf("MDSERVER[%02d]: new cache entries received, insert %d bytes in local cache\n",
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
      debug_printf("MDSERVER[%02d]: END message received dont listen to parent from now\n",
		   ldcs_process_data->md_rank,msg->header.len);
      printf("MDSERVER[%02d]: END message received \n", ldcs_process_data->md_rank);
      
      ldcs_audit_server_md_unregister_fd ( ldcs_process_data );
      
      ldcs_process_data->md_size=1;

      _ldcs_client_process_clients_requests_after_end( ldcs_process_data );
    }
    break;
    
  default: ;
    {
      debug_printf("MDSERVER[%03d]: recvd unknown message of type: %s len=%d data=%s ...\n", 
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
  ldcs_msg_free(&msg);

  ldcs_process_data->server_stat.md_cb.cnt++;
  ldcs_process_data->server_stat.md_cb.time+=(ldcs_get_time()-cb_starttime);

  return(rc);
}


int _ldcs_audit_server_md_cobo_from_fe_CB ( int fd, int nc, void *data ) {
  int rc=0;
  ldcs_process_data_t *ldcs_process_data = ( ldcs_process_data_t *) data ;
  ldcs_message_t *in_msg=ldcs_msg_new();
  ldcs_message_t out_msg;

  double cb_starttime;
  cb_starttime=ldcs_get_time();
  
  out_msg.header.len=0;
  out_msg.alloclen=0;
  out_msg.data=NULL;

  /* receive msg from fe */
  _ldcs_audit_server_md_cobo_recv_msg ( fd, in_msg );

  /* statistics */
  ldcs_process_data->client_table[nc].query_arrival_time=cb_starttime;
    
  /* process message */
  switch(in_msg->header.type) {
  case LDCS_MSG_PRELOAD_FILE:
    {
      debug_printf("MDSERVER[%02d]: new preload file name received %d (%s)\n",
		   ldcs_process_data->md_rank,in_msg->header.len, in_msg->data);
      
      rc=_ldcs_client_process_fe_preload_requests ( ldcs_process_data, in_msg->data );

      /* send ack */
      if(rc==1) {
	out_msg.header.type=LDCS_MSG_PRELOAD_FILE_OK;
      } else {
	out_msg.header.type=LDCS_MSG_PRELOAD_FILE_NOT_FOUND;
      }
      _ldcs_audit_server_md_cobo_send_msg(fd, &out_msg);

    }
    break;
     
  default: ;
    {
      debug_printf("MDSERVER[%03d]: recvd unknown message of type: %s len=%d data=%s ...\n", 
		   ldcs_process_data->md_rank,
		   _message_type_to_str(in_msg->header.type),
		   in_msg->header.len, in_msg->data );
      printf("MDSERVER[%03d]: recvd unknown message of type: %s len=%d data=%s ...\n", 
	     ldcs_process_data->md_rank,
	     _message_type_to_str(in_msg->header.type),
	     in_msg->header.len, in_msg->data );
      _error("wrong message");
    }
    break;
  }

  /* statistic */
  ldcs_process_data->server_stat.preload.cnt++;
  ldcs_process_data->server_stat.preload.bytes+=in_msg->header.len;
  ldcs_process_data->server_stat.preload.time+=(ldcs_get_time()-
						ldcs_process_data->client_table[nc].query_arrival_time);

  return(rc);
}


int _ldcs_audit_server_md_cobo_bcast_msg ( ldcs_process_data_t *ldcs_process_data, ldcs_message_t *msg ) {
  int rc=0;
  double starttime;
  starttime=ldcs_get_time();
  
  rc=cobo_bcast(&msg->header,sizeof(msg->header),0);
  if(rc!=COBO_SUCCESS) {
    debug_printf("MDSERVER[%02d]: got rc=%d from cobo_bcast (failed)\n", ldcs_process_data->md_rank, rc);
  }
  debug_printf("MDSERVER[%02d]: got rc=%d from cobo_bcast msg.type=%s\n", ldcs_process_data->md_rank, rc,
	       _message_type_to_str(msg->header.type));
  
  if(msg->header.len>0) {

    if(ldcs_process_data->md_rank!=0) { 
      if(msg->header.len>msg->alloclen) {
	debug_printf("MDSERVER[%02d]: alloc memory for message data: %d bytes\n", ldcs_process_data->md_rank, msg->header.len);
	msg->data = (char *) malloc(msg->header.len);
	msg->alloclen=msg->header.len;
	if (!msg->data)  _error("could not allocate memory for message data");
      } else {
	debug_printf("MDSERVER[%02d]: memory already allocated for message data: %d bytes alloc=%d bytes\n", ldcs_process_data->md_rank, msg->header.len,msg->alloclen);
      }
    } else {
	debug_printf("MDSERVER[%02d]: root node no need to allocate memory for message data: %d bytes alloc=%d bytes\n", ldcs_process_data->md_rank, msg->header.len,msg->alloclen);
    }
    rc=cobo_bcast(msg->data,msg->header.len,0);
    if(rc!=COBO_SUCCESS) {
      debug_printf("MDSERVER[%02d]: got rc=%d from cobo_bcast\n", ldcs_process_data->md_rank, rc);
    }
  } else {
    msg->data=NULL;
  }

  /* statistic */
  ldcs_process_data->server_stat.bcast.cnt++;
  ldcs_process_data->server_stat.bcast.bytes+=msg->header.len;
  ldcs_process_data->server_stat.bcast.time+=(ldcs_get_time()-starttime);
  
  return(rc);
}

int _ldcs_audit_server_md_cobo_send_msg(int fd, ldcs_message_t * msg) {

  int n;

  debug_printf("MDSERVER: sending message of type: %s len=%d data=%s ...\n",
	       _message_type_to_str(msg->header.type),
	       msg->header.len,msg->data );  
  
  n = ldcs_cobo_write_fd(fd, &msg->header,sizeof(msg->header));
  if (n < 0) _error("ERROR writing header to pipe");
  debug_printf("MDSERVER: got rc=%d from ldcs_cobo_write_fd msg.type=%s\n", n,
	       _message_type_to_str(msg->header.type));

  if(msg->header.len>0) {
    n = ldcs_cobo_write_fd(fd,(void *) msg->data, msg->header.len);
    if (n < 0) _error("ERROR writing data to pipe");
    if (n != msg->header.len) _error("sent different number of bytes for message data");
  }
  debug_printf("MDSERVER: got rc=%d from ldcs_cobo_write_fd msg.type=%s\n", n,
	       _message_type_to_str(msg->header.type));
    
  return(0);
}

int _ldcs_audit_server_md_cobo_recv_msg ( int fd, ldcs_message_t *msg ) {
  int rc=0;
  
  rc=ldcs_cobo_read_fd(fd, &msg->header,sizeof(msg->header));
  if(rc!=sizeof(msg->header)) {
    debug_printf("MDSERVER: got rc=%d from ldcs_cobo_read_fd (failed)\n", rc);
  }
  debug_printf("MDSERVER: got rc=%d from ldcs_cobo_read_fd msg.type=%s\n", rc,
	       _message_type_to_str(msg->header.type));
  
  if(msg->header.len>0) {

    if(msg->header.len>msg->alloclen) {
      debug_printf("MDSERVER: alloc memory for message data: %d bytes\n", msg->header.len);
      msg->data = (char *) malloc(msg->header.len);
      msg->alloclen=msg->header.len;
      if (!msg->data)  _error("could not allocate memory for message data");
    } else {
      debug_printf("MDSERVER: memory already allocated for message data: %d bytes alloc=%d bytes\n", msg->header.len,msg->alloclen);
    }
    
    rc=ldcs_cobo_read_fd(fd, msg->data, msg->header.len);
    if(rc!=msg->header.len) {
      debug_printf("MDSERVER: got rc=%d from ldcs_cobo_read_fd (failed)\n", rc);
    } else {
      debug_printf("MDSERVER: got rc=%d from ldcs_cobo_read_fd msg.type=%s\n", rc,
		   _message_type_to_str(msg->header.type));
    }

  } else {
    msg->data=NULL;
  }
  
  return(rc);
}



int ldcs_audit_server_fe_md_open ( char **hostlist, int numhosts, void **data  ) {
  int rc=0;
  int num_ports   = 20;
  int* portlist = malloc(num_ports * sizeof(int));
  int i;
  int root_fd, ack;

  /* build our portlist */
  for (i=0; i<num_ports; i++) {
    portlist[i] = 5000 + i;
  }

  cobo_server_open(2384932, hostlist, numhosts, portlist, num_ports);

  cobo_server_get_root_socket(&root_fd);
  
  ldcs_cobo_read_fd(root_fd, &ack, sizeof(ack));
  printf("server_rsh_ldcs: got ack=%d from tree root\n",ack);

  return(rc);
}

int ldcs_audit_server_fe_md_preload ( char *filename, void *data  ) {
  int rc=0;
  char buffer[MAX_PATH_LEN];
  FILE * fp;
  char * line = NULL, *p;
  size_t len = 0;
  ssize_t read;
  ldcs_message_t out_msg;
  ldcs_message_t *in_msg=ldcs_msg_new();
  int root_fd;
  
  fprintf(stderr,"SERVERFE: open file for list of library for preload: '%s'\n",filename); 
  fp = fopen(filename, "r");   if (fp == NULL)  perror("could not open preload file");

  out_msg.header.type=LDCS_MSG_PRELOAD_FILE;
  out_msg.alloclen=MAX_PATH_LEN;  out_msg.data=buffer;
  cobo_server_get_root_socket(&root_fd);

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
    
    _ldcs_audit_server_md_cobo_send_msg(root_fd, &out_msg);
    
    fprintf(stderr, "SERVERFE: sent preload msg(%s)  to tree root\n",out_msg.data);

    /* receive msg from root node */
    _ldcs_audit_server_md_cobo_recv_msg ( root_fd, in_msg );
    if(in_msg->header.type==LDCS_MSG_PRELOAD_FILE_OK) {
      fprintf(stderr, "SERVERFE:   --> successful\n");
    } else {
      fprintf(stderr, "SERVERFE:   --> not successful\n");
    }

  }
  fclose(fp);
  if (line) free(line);

  return(rc);
}

int ldcs_audit_server_fe_md_close ( void *data  ) {
  int rc=0;
  int root_fd, ack;
  cobo_server_get_root_socket(&root_fd);
  
  ldcs_cobo_read_fd(root_fd, &ack, sizeof(ack));
  printf("server_rsh_ldcs: got ack=%d from tree root\n",ack);
  
  ack=15;
  ldcs_cobo_write_fd(root_fd, &ack, sizeof(ack));
  printf("server_rsh_ldcs: sent ack=%d to tree root\n",ack);

  /* open and close the server */
  cobo_server_close();


  return(rc);
}
