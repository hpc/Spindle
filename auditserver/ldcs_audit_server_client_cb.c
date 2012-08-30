#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/inotify.h>
#include <errno.h>

#include "ldcs_api.h"
#include "ldcs_audit_server_process.h"
#include "ldcs_audit_server_md.h"
#include "ldcs_audit_server_filemngt.h"
#include "ldcs_audit_server_stateloop.h"
#include "ldcs_api_listen.h"
#include "ldcs_cache.h" 
#define DISTCACHE 1


ldcs_state_t _ldcs_client_dump_info ( ldcs_process_data_t *ldcs_process_data );

/* some message container */
static  char buffer_in[MAX_PATH_LEN];
/* static  char buffer_out[MAX_PATH_LEN]; */

int _ldcs_client_CB ( int fd, int nc, void *data ) {
  int rc=0;
  ldcs_process_data_t *ldcs_process_data = ( ldcs_process_data_t *) data ;
  ldcs_message_t in_msg;
  int connid;
  double starttime,msg_time,cb_starttime;
  cb_starttime=ldcs_get_time();

  ldcs_process_data->last_action_on_nc=nc;

  connid=ldcs_process_data->client_table[nc].connid;
  debug_printf("starting callback for fd=%d nc=%d connid=%d lfd=%d\n",fd,nc,connid,ldcs_get_fd(connid));
  in_msg.header.type=LDCS_MSG_UNKNOWN;  in_msg.alloclen=MAX_PATH_LEN;  in_msg.header.len=0;  in_msg.data=buffer_in;

  /* get message from client */
  starttime=ldcs_get_time();
  ldcs_recv_msg_static(connid,&in_msg, LDCS_READ_BLOCK);
  msg_time=ldcs_get_time()-starttime;

  /* printf("SERVER[%03d]: received message on connection connid=%d\n", nc, connid); */
  debug_printf("received message on connection nc=%d connid=%d\n", nc, connid);

  /* prevent infinite loop */
  if(in_msg.header.len==0) {
    debug_printf("connid=%d null_msg_cnt = %d\n",connid, ldcs_process_data->client_table[nc].null_msg_cnt);
    ldcs_process_data->client_table[nc].null_msg_cnt++;
  } else {
    ldcs_process_data->client_table[nc].null_msg_cnt=0;
  }
  if(ldcs_process_data->client_table[nc].null_msg_cnt>8) {
    debug_printf(" too many null byte messages on conn %d, closing connection\n",connid);
    printf("SERVER[%03d]: too many null byte messages: closing connection\n", nc);
    ldcs_close_server_connection(connid);
    ldcs_process_data->client_table[nc].state = LDCS_CLIENT_STATUS_FREE;
    ldcs_process_data->client_table_used--;
    ldcs_listen_unregister_fd(fd);
  }

  /* statistics */
  ldcs_process_data->client_table[nc].query_arrival_time=cb_starttime;

  /* process message */
  switch(in_msg.header.type) {
  case LDCS_MSG_END:
    ldcs_server_process_state( ldcs_process_data, &in_msg, LDCS_STATE_CLIENT_END_MSG );
    break;
    
  case LDCS_MSG_CWD:
  case LDCS_MSG_HOSTNAME:
  case LDCS_MSG_PID:
  case LDCS_MSG_LOCATION:
    ldcs_server_process_state( ldcs_process_data, &in_msg, LDCS_STATE_CLIENT_INFO_MSG );
    break;

  case LDCS_MSG_MYRANKINFO_QUERY:
    ldcs_server_process_state( ldcs_process_data, &in_msg, LDCS_STATE_CLIENT_MYRANKINFO_MSG );
    break;
      
  case LDCS_MSG_FILE_QUERY:
    ldcs_server_process_state( ldcs_process_data, &in_msg, LDCS_STATE_CLIENT_FILE_QUERY_MSG );
    break;

  case LDCS_MSG_FILE_QUERY_EXACT_PATH:
    ldcs_server_process_state( ldcs_process_data, &in_msg, LDCS_STATE_CLIENT_FILE_QUERY_EXACT_PATH_MSG );
    break;

  default: ;
    debug_printf("SERVER[%03d]: recvd unknown message of type: %s len=%d data=%s ...\n", nc,
	   _message_type_to_str(in_msg.header.type),
	   in_msg.header.len, in_msg.data );
    printf("SERVER[%03d]: recvd unknown message of type: %s len=%d data=%s ...\n", nc,
	   _message_type_to_str(in_msg.header.type),
	   in_msg.header.len, in_msg.data );
    break;
  }
  
  debug_printf("leaving callback for fd %d nc = %d\n",fd,nc);

  ldcs_process_data->server_stat.client_cb.cnt++;
  ldcs_process_data->server_stat.client_cb.time+=(ldcs_get_time()-cb_starttime);
	  
  return(rc);
}

int _ldcs_client_process_clients_requests_after_update ( ldcs_process_data_t *ldcs_process_data ) {
  int rc=0;
  int nc;

  for(nc=0;nc<ldcs_process_data->client_table_used;nc++) {
    if(ldcs_process_data->client_table[nc].query_open) {

      ldcs_process_data->last_action_on_nc=nc;
      ldcs_server_process_state( ldcs_process_data, NULL, LDCS_STATE_CLIENT_FILE_QUERY_CHECK );
      
    }
  }

  return(rc);
}

int _ldcs_client_process_clients_requests_after_end ( ldcs_process_data_t *ldcs_process_data ) {
  int rc=0;
  int nc;

  for(nc=0;nc<ldcs_process_data->client_table_used;nc++) {
    if(ldcs_process_data->client_table[nc].query_open) {

      ldcs_process_data->last_action_on_nc=nc;
      ldcs_server_process_state( ldcs_process_data, NULL, LDCS_STATE_CLIENT_FILE_QUERY_CHECK );
      
    }
  }

  return(rc);
}

int _ldcs_client_process_fe_preload_requests ( ldcs_process_data_t *ldcs_process_data, char *fn ) {
  int rc=0;
  int nc=0;
  char *filename=NULL, *dirname=NULL, *tmpname, *globalpath=NULL, *localpath=NULL;
  char *fe_cwd=""; 		
  ldcs_cache_result_t cache_filedir_result=LDCS_CACHE_UNKNOWN;
  double cb_starttime;

  debug_printf(" got preload request for %s\n", fn);

  /* TDB: transfer CWD path to server */
  strncpy(ldcs_process_data->client_table[nc].remote_cwd,(fe_cwd)?fe_cwd:"\0",MAX_PATH_LEN);

        
  rc=parseFilenameExact(fn, ldcs_process_data->client_table[nc].remote_cwd, &filename, &dirname);
  strncpy(ldcs_process_data->client_table[nc].query_filename,(filename)?filename:"\0",MAX_PATH_LEN);
  strncpy(ldcs_process_data->client_table[nc].query_dirname,(dirname)?dirname:"\0",MAX_PATH_LEN);
  tmpname=concatStrings(dirname,strlen(dirname),filename,strlen(filename));
  strncpy(ldcs_process_data->client_table[nc].query_globalpath,tmpname,MAX_PATH_LEN);
  free(tmpname);
  strncpy(ldcs_process_data->client_table[nc].query_localpath,"",MAX_PATH_LEN);
  ldcs_process_data->client_table[nc].query_open = 1;
  ldcs_process_data->client_table[nc].query_forwarded=0;
  ldcs_process_data->client_table[nc].query_exact_path= 1;
  if(filename) free(filename);
  if(dirname) free(dirname);
  
  ldcs_process_data->last_action_on_nc=nc;
  ldcs_server_process_state( ldcs_process_data, NULL, LDCS_STATE_CLIENT_FILE_QUERY_CHECK );
  
  /* chech success */
  cache_filedir_result=ldcs_cache_findFileDirInCache(ldcs_process_data->client_table[nc].query_filename,
						     ldcs_process_data->client_table[nc].query_dirname, &globalpath, &localpath);

  if(cache_filedir_result==LDCS_CACHE_FILE_FOUND) {
    rc=1;
    free(globalpath);free(localpath);
  } else {
    rc=0;
  }

  return(rc);
}

ldcs_state_t _ldcs_client_dump_info ( ldcs_process_data_t *ldcs_process_data ) {
  int nc, connid;
  int rc=0;

  for(nc=0;nc<ldcs_process_data->client_table_used;nc++) {
    connid=ldcs_process_data->client_table[nc].connid;
    debug_printf("  CLIENT DUMP: nc = %2d, connid=%d fd=%d\n",nc,
		 ldcs_process_data->client_table[nc].connid,
		 ldcs_get_fd(ldcs_process_data->client_table[nc].connid));
    debug_printf("               pid= %6d, hostname=%s, cwd=%s fd=%d\n",
		 ldcs_process_data->client_table[nc].remote_pid,
		 ldcs_process_data->client_table[nc].remote_hostname,
		 ldcs_process_data->client_table[nc].remote_cwd, 
		 ldcs_get_fd(connid));
  }
  return(rc);
}
