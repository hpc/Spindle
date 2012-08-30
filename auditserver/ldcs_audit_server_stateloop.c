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
#include "ldcs_audit_server_stateloop.h"

  /* printf(format, __VA_ARGS__ ); \ */

#define log_msg(format, ...) \
  do {                       \
  debug_printf(format, __VA_ARGS__ ); \
} while (0)
 

/* some message container */
/* static  char buffer_in[MAX_PATH_LEN]; */
static  char buffer_out[MAX_PATH_LEN];

/* ******************************* */
/* ******************************* */
/* ******************************* */
ldcs_state_t ldcs_server_process_state ( ldcs_process_data_t *ldcs_process_data, 
                                         ldcs_message_t *msg, 
                                         ldcs_state_t state ) {
  int rc=0;
  int nc = ldcs_process_data->last_action_on_nc; 
  int count=0;

  while(state!=LDCS_STATE_READY) {
    
    count++;
    debug_printf("STATE[#%d]=%30s nc=%d start \n",count,_state_type_to_str(state),nc);
    if(count>100) break;

    switch(state) {

      /* Info message from Client received */
    case LDCS_STATE_CLIENT_INFO_MSG:
      {
        if(msg->header.type == LDCS_MSG_CWD) {
          strncpy(ldcs_process_data->client_table[nc].remote_cwd,msg->data,MAX_PATH_LEN);
          log_msg("SERVER[%03d][C%02d]: recvd CWD: '%-s'\n", ldcs_process_data->md_rank, nc, msg->data );
        } 
        if(msg->header.type == LDCS_MSG_HOSTNAME) {
          strncpy(ldcs_process_data->client_table[nc].remote_hostname,msg->data,MAX_PATH_LEN);
          log_msg("SERVER[%03d][C%02d]: recvd HOSTNAME: '%-s'\n", ldcs_process_data->md_rank, nc, msg->data );
        }
        if(msg->header.type == LDCS_MSG_PID) {
          int mypid;
          sscanf(msg->data,"%d",&mypid);
          ldcs_process_data->client_table[nc].remote_pid=mypid;
          log_msg("SERVER[%03d][C%02d]: recvd PID: '%d'\n", ldcs_process_data->md_rank, nc, mypid );
        } 
        if(msg->header.type == LDCS_MSG_LOCATION) {
          strncpy(ldcs_process_data->client_table[nc].remote_location,msg->data,MAX_PATH_LEN);
          log_msg("SERVER[%03d][C%02d]: recvd LOCATION: '%-s'\n", ldcs_process_data->md_rank, nc, msg->data );
        }
        state=LDCS_STATE_READY;
        break;
      }

      /* rank info query message from client received */
    case LDCS_STATE_CLIENT_MYRANKINFO_MSG:
      {
	int tmpdata[4];
        ldcs_message_t out_msg;
        int connid;

        if(msg->header.type == LDCS_MSG_MYRANKINFO_QUERY) {
	  tmpdata[0]=ldcs_process_data->client_table[nc].lrank;
	  tmpdata[1]=ldcs_process_data->client_counter;	/* only the current size, not the overall size */
	  tmpdata[2]=ldcs_process_data->md_rank;
	  tmpdata[3]=ldcs_process_data->md_size;
          log_msg("SERVER[%03d][C%02d]: recvd MYRANKINFO_QUERY: --> (%d,%d) (%d,%d) \n", ldcs_process_data->md_rank, nc, tmpdata[0], tmpdata[1], tmpdata[2], tmpdata[3]);
        } 

        connid=ldcs_process_data->client_table[nc].connid;
        out_msg.header.type=LDCS_MSG_MYRANKINFO_QUERY_ANSWER;
        out_msg.alloclen=MAX_PATH_LEN;  out_msg.data=buffer_out;
	out_msg.header.len=sizeof(tmpdata);
	memcpy(out_msg.data,&tmpdata,out_msg.header.len);

        /* statistic */
        ldcs_process_data->server_stat.clientmsg.cnt++;
        ldcs_process_data->server_stat.clientmsg.time+=(ldcs_get_time()-
                                                        ldcs_process_data->client_table[nc].query_arrival_time);
        ldcs_send_msg(connid,&out_msg);

        state=LDCS_STATE_READY;
        break;
      }

      /* ****************************************** */
      /*  ACTIONS ON FILES                          */
      /* ****************************************** */


      /* Query message from Client received */
    case LDCS_STATE_CLIENT_FILE_QUERY_EXACT_PATH_MSG:
      {
        /* do initial check of query, parse the filename and store info */
        char *filename=NULL, *dirname=NULL, *tmpname;
        
        rc=parseFilenameExact(msg->data, ldcs_process_data->client_table[nc].remote_cwd, &filename, &dirname);
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

        log_msg("SERVER[%03d][C%02d]: recvd QUERY (exact): '%s'\n", ldcs_process_data->md_rank, nc, msg->data );

        /* switch to next state */
        state=LDCS_STATE_CLIENT_FILE_QUERY_CHECK;
      }
      break;

    case LDCS_STATE_CLIENT_FILE_QUERY_MSG:
      {
        /* do initial check of query, parse the filename and store info */
        char *filename=NULL, *dirname=NULL, *tmpname;
        rc=parseFilename(msg->data, ldcs_process_data->client_table[nc].remote_cwd, &filename, &dirname);
        strncpy(ldcs_process_data->client_table[nc].query_filename,(filename)?filename:"\0",MAX_PATH_LEN);
        strncpy(ldcs_process_data->client_table[nc].query_dirname,(dirname)?dirname:"\0",MAX_PATH_LEN);
	tmpname=concatStrings(dirname,strlen(dirname),filename,strlen(filename));
        strncpy(ldcs_process_data->client_table[nc].query_globalpath,tmpname,MAX_PATH_LEN);
	free(tmpname);
        strncpy(ldcs_process_data->client_table[nc].query_localpath,"",MAX_PATH_LEN);
        ldcs_process_data->client_table[nc].query_open = 1;
        ldcs_process_data->client_table[nc].query_forwarded=0;
        ldcs_process_data->client_table[nc].query_exact_path= 0;
	if(filename) free(filename);
	if(dirname) free(dirname);

        log_msg("SERVER[%03d][C%02d]: recvd QUERY: '%s'\n", ldcs_process_data->md_rank, nc, msg->data );

        /* switch to next state */
        if(!dirname) {
          state=LDCS_STATE_CLIENT_PROCESS_NODIR_QUERY;
        } else {
          state=LDCS_STATE_CLIENT_FILE_QUERY_CHECK;
        }
      }
      break;


      /* Test file request   */
    case LDCS_STATE_CLIENT_FILE_QUERY_CHECK:
      {
        char *globalpath, *localpath=NULL;
        int responsible=0;
        ldcs_cache_result_t cache_dir_result=LDCS_CACHE_UNKNOWN;
        ldcs_cache_result_t cache_filedir_result=LDCS_CACHE_UNKNOWN;
        ldcs_cache_result_t cache_filename_result=LDCS_CACHE_UNKNOWN;

	/* check directory + file */
        cache_filedir_result=ldcs_cache_findFileDirInCache(ldcs_process_data->client_table[nc].query_filename,
                                                           ldcs_process_data->client_table[nc].query_dirname, &globalpath, &localpath);
        debug_printf(" CACHE: check file+dirname: '%s'/'%s' -> '%d' (%s,%s) \n", 
                     ldcs_process_data->client_table[nc].query_dirname, 
                     ldcs_process_data->client_table[nc].query_filename, 
                     cache_filedir_result==LDCS_CACHE_FILE_FOUND,globalpath,localpath);

        if(! ldcs_process_data->client_table[nc].query_exact_path) {
          /* not exact path query */

          if(cache_filedir_result!=LDCS_CACHE_FILE_FOUND) {
        
	    /* check is file anywhere else (prefer entries having set localpath) */
	    cache_filename_result=ldcs_cache_findFileInCachePrio(ldcs_process_data->client_table[nc].query_filename, &globalpath, &localpath);
	    debug_printf(" CACHE: check for another location filename: '%s' -> '%d' (%s,%s) \n", 
			 ldcs_process_data->client_table[nc].query_filename, 
			 cache_filename_result==LDCS_CACHE_FILE_FOUND,globalpath,localpath);

	    if(cache_filename_result==LDCS_CACHE_FILE_FOUND) {
	      /* re-initialize query to this file */
	      char *filename=NULL, *dirname=NULL;
	      rc=parseFilename(globalpath, ldcs_process_data->client_table[nc].remote_cwd, &filename, &dirname);

	      strncpy(ldcs_process_data->client_table[nc].query_filename,(filename)?filename:"\0",MAX_PATH_LEN);
	      strncpy(ldcs_process_data->client_table[nc].query_dirname,(dirname)?dirname:"\0",MAX_PATH_LEN);
	      strncpy(ldcs_process_data->client_table[nc].query_globalpath,globalpath,MAX_PATH_LEN);
	      if(filename) free(filename);
	      if(dirname) free(dirname);
	      
	      cache_filedir_result=LDCS_CACHE_FILE_FOUND; /* proceed like found directly  */

	      debug_printf(" CACHE: re-initialize query to: '%s'/'%s' (%s) \n", 
			   ldcs_process_data->client_table[nc].query_dirname,
			   ldcs_process_data->client_table[nc].query_filename, 
			   ldcs_process_data->client_table[nc].query_globalpath);
	      
	    } 
	  }
	}

	/* file found? */
	if(cache_filedir_result==LDCS_CACHE_FILE_FOUND) {

	  /* check if file is stored locally */
	  if(localpath) strncpy(ldcs_process_data->client_table[nc].query_localpath,localpath,MAX_PATH_LEN);

	} else {
	  
	  /* check state of directory */
	  cache_dir_result=ldcs_cache_findDirInCache(ldcs_process_data->client_table[nc].query_dirname);
	  debug_printf(" CACHE: check dirname: '%s' -> '%d' \n", ldcs_process_data->client_table[nc].query_dirname, 
		       cache_dir_result==LDCS_CACHE_DIR_PARSED_AND_EXISTS);
	}

	responsible=ldcs_audit_server_md_is_responsible(ldcs_process_data,
                                                        ldcs_process_data->client_table[nc].query_filename);

        /* switch to next state */
	if(ldcs_process_data->client_table[nc].query_exact_path) {

          /* file info in cache? */
          if(cache_filedir_result==LDCS_CACHE_FILE_FOUND) {
            if(localpath) {
              state=LDCS_STATE_CLIENT_PROCESS_FULFILLED_QUERY;
            } else {
              /* file exists but not in local path (mfn) --> process file */
              if(responsible) state=LDCS_STATE_CLIENT_PROCESS_FILE;
              else            state=LDCS_STATE_CLIENT_FORWARD_QUERY;
            } 
          } else {
            if(cache_dir_result==LDCS_CACHE_DIR_NOT_PARSED) {
              /* process directory (local or remote) and come back */
              if(responsible) state=LDCS_STATE_CLIENT_PROCESS_DIR;
              else            state=LDCS_STATE_CLIENT_FORWARD_QUERY;
              
            } else { 
              /* file is not in this directory */
              state=LDCS_STATE_CLIENT_PROCESS_REJECTED_QUERY; 
            }
          }
        } else {
          /* not exact path query */

          if(cache_filedir_result==LDCS_CACHE_FILE_FOUND) {

            if(localpath) {
              state=LDCS_STATE_CLIENT_PROCESS_FULFILLED_QUERY;
            } else {
              /* file not in local path, but found --> process file */
              if(responsible) state=LDCS_STATE_CLIENT_PROCESS_FILE;
              else            state=LDCS_STATE_CLIENT_FORWARD_QUERY;
	    } 
          } else {

            if(cache_dir_result==LDCS_CACHE_DIR_NOT_PARSED) {
              /* process directory (local or remote) and come back */
              if(responsible) state=LDCS_STATE_CLIENT_PROCESS_DIR;
              else            state=LDCS_STATE_CLIENT_FORWARD_QUERY;
            } else {
              /* directory parsed, but file not found */
	      /* reject and wait for a client poviding another new path */
              if(responsible) state=LDCS_STATE_CLIENT_PROCESS_REJECTED_QUERY; 
              else            state=LDCS_STATE_CLIENT_FORWARD_QUERY;
	    }
          }
          
        }
        if(localpath) free(localpath);
        if(globalpath) free(globalpath);
      }
      break;
      
      /* responsible for file -> process it   */
    case LDCS_STATE_CLIENT_PROCESS_DIR:
      {
        ldcs_cache_result_t cache_dir_result;
        double starttime;

        /* check directory */
        cache_dir_result=ldcs_cache_findDirInCache(ldcs_process_data->client_table[nc].query_dirname);

        if(cache_dir_result==LDCS_CACHE_DIR_NOT_PARSED) {

          /* process directory */
          starttime=ldcs_get_time();
          cache_dir_result=ldcs_cache_processDirectory(ldcs_process_data->client_table[nc].query_dirname);
          ldcs_process_data->server_stat.procdir.cnt++;
          ldcs_process_data->server_stat.procdir.bytes+=rc;
          ldcs_process_data->server_stat.procdir.time+=(ldcs_get_time()-starttime);

          log_msg("SERVER[%03d][C%02d]: process dir: '%s'\n", ldcs_process_data->md_rank, nc, 
                  ldcs_process_data->client_table[nc].query_dirname );
        
	  
          if ( (cache_dir_result==LDCS_CACHE_DIR_PARSED_AND_EXISTS) ||
	       (cache_dir_result==LDCS_CACHE_DIR_PARSED_AND_NOT_EXISTS)
	       ) {
	    /* also distribution of hash entry required if dir does not exists, otherwise deadlocks possible when doing preload */

            if(ldcs_audit_server_md_distribution_required(ldcs_process_data,
                                                          ldcs_process_data->client_table[nc].query_filename)) {
              /* distribute info about directory to other server */
              ldcs_message_t* new_entries_msg=ldcs_msg_new();
              char *new_entries_data;
              int new_entries_data_len;
            
              new_entries_msg->header.type=LDCS_MSG_CACHE_ENTRIES;
            
              rc=ldcs_cache_getNewEntriesSerList(&new_entries_data,&new_entries_data_len);
            
              if((rc==0) &&  (new_entries_data_len>0)) {
                new_entries_msg->data=new_entries_data;
                new_entries_msg->header.len=new_entries_data_len;
                new_entries_msg->alloclen=new_entries_data_len;
                ldcs_audit_server_md_distribute(ldcs_process_data, new_entries_msg);
              }
              ldcs_msg_free(&new_entries_msg);
            }
          }
        }

        /* switch to next state */

        /* go back to file query */
        state=LDCS_STATE_CLIENT_FILE_QUERY_CHECK;

        break;
      }
    
      /* process file, copy it into local memory and distribute   */
    case LDCS_STATE_CLIENT_PROCESS_FILE:
      {

        /* copy file to local memory and distribute file to other server */
        ldcs_message_t* file_msg=ldcs_msg_new();
        char *newfilename, *newdirname, *localpath; 
        double starttime;
        int domangle;
        /* read file */
        starttime=ldcs_get_time();
        file_msg->header.type=LDCS_MSG_FILE_DATA;
        domangle=ldcs_process_data->client_table[nc].query_exact_path;

        rc=ldcs_audit_server_filemngt_read_file(ldcs_process_data->client_table[nc].query_filename,
                                                ldcs_process_data->client_table[nc].query_dirname,  
                                                ldcs_process_data->client_table[nc].query_globalpath, 
                                                domangle, file_msg);
        ldcs_process_data->server_stat.libread.cnt++;
        ldcs_process_data->server_stat.libread.bytes+=rc;
        ldcs_process_data->server_stat.libread.time+=(ldcs_get_time()-starttime);
        
        /* store file */
        starttime=ldcs_get_time();
        rc=ldcs_audit_server_filemngt_store_file(file_msg, &newfilename, &newdirname,  &localpath, &domangle); /* store files and returns file info */

	strncpy(ldcs_process_data->client_table[nc].query_localpath,localpath,MAX_PATH_LEN);

        ldcs_process_data->server_stat.libstore.cnt++;
        ldcs_process_data->server_stat.libstore.bytes+=rc;
        ldcs_process_data->server_stat.libstore.time+=(ldcs_get_time()-starttime);
        
        /* update cache file */
        ldcs_cache_updateLocalPath(ldcs_process_data->client_table[nc].query_filename,
                                   ldcs_process_data->client_table[nc].query_dirname, localpath);
        ldcs_cache_updateStatus(ldcs_process_data->client_table[nc].query_filename,
                                ldcs_process_data->client_table[nc].query_dirname, LDCS_CACHE_OBJECT_STATUS_LOCAL_PATH);

        log_msg("SERVER[%03d][C%02d]:  store file: '%s' -> '%s'\n", ldcs_process_data->md_rank, nc, 
                ldcs_process_data->client_table[nc].query_globalpath, ldcs_process_data->client_table[nc].query_localpath );
	
        /* distribute file data */
        if(ldcs_audit_server_md_distribution_required(ldcs_process_data,
                                                      ldcs_process_data->client_table[nc].query_filename)) {
          starttime=ldcs_get_time();
          ldcs_audit_server_md_distribute(ldcs_process_data, file_msg);
          ldcs_process_data->server_stat.libdist.cnt++;
          ldcs_process_data->server_stat.libdist.bytes+=file_msg->header.len;
          ldcs_process_data->server_stat.libdist.time+=(ldcs_get_time()-starttime);
        }
        
        ldcs_msg_free(&file_msg);
        free(newfilename);
        free(newdirname);
        free(localpath);

        /* initiate update on all clients which have open queries  */
        ldcs_process_data->update=LDCS_UPDATE_STATE_INITIATE;
                
        /* switch to next state */
        state=LDCS_STATE_CLIENT_PROCESS_FULFILLED_QUERY;
        break;
      }
    
      /* Test file request   */
    case LDCS_STATE_CLIENT_FORWARD_QUERY:
      {

        int md_multi = ldcs_audit_server_md_distribution_required(ldcs_process_data,
                                                                  ldcs_process_data->client_table[nc].query_filename);

        if(ldcs_process_data->client_table[nc].query_forwarded==0) {
          
          /* MD: send query to server who is responsible */
          if(md_multi) {
            
            /* keep request */
            debug_printf(" CACHE: store for later check: nc=%d '%s'/'%s' %s \n", nc, 
                         ldcs_process_data->client_table[nc].query_dirname, 
                         ldcs_process_data->client_table[nc].query_filename,
                         ldcs_process_data->client_table[nc].query_globalpath);
            ldcs_process_data->client_table[nc].query_open = 1;
            ldcs_process_data->client_table[nc].query_forwarded=1;
            ldcs_audit_server_md_forward_query(ldcs_process_data, msg);
          }
        }

        /* switch to next state */
        if(md_multi) {
          state=LDCS_STATE_DONE;
        } else {
          state=LDCS_STATE_CLIENT_PROCESS_REJECTED_QUERY;
        }
        break;
      }


      /* ****************************************** */
      /* REPLY ACTIONS TO CLIENT                    */
      /* ****************************************** */

      /* only filename --> no result  */
    case LDCS_STATE_CLIENT_PROCESS_FULFILLED_QUERY:
      {
        ldcs_message_t out_msg;
        int connid;
	char *remote_localpath;
        out_msg.header.type=LDCS_MSG_FILE_QUERY_ANSWER;
        out_msg.alloclen=MAX_PATH_LEN;  out_msg.data=buffer_out;

        connid=ldcs_process_data->client_table[nc].connid;

	/*translate to remote local path */
	remote_localpath=replacePatString(ldcs_process_data->client_table[nc].query_localpath,ldcs_process_data->location,ldcs_process_data->client_table[nc].remote_location);	
	strncpy(out_msg.data,remote_localpath,MAX_PATH_LEN);
        out_msg.header.len=strlen(remote_localpath); 
	free(remote_localpath);

	/* send answer only to active client not to pseudo client */
	if ( (ldcs_process_data->client_table[nc].state==LDCS_CLIENT_STATUS_ACTIVE) && 
	     (connid>=0) )  {

	  ldcs_send_msg(connid,&out_msg);
	  ldcs_process_data->client_table[nc].query_open=0;
	  log_msg("SERVER[%03d][C%02d]: answer query (fulfilled): '%s'\n", ldcs_process_data->md_rank, nc, out_msg.data);

	  /* statistic */
	  ldcs_process_data->server_stat.clientmsg.cnt++;
	  ldcs_process_data->server_stat.clientmsg.time+=(ldcs_get_time()-
							  ldcs_process_data->client_table[nc].query_arrival_time);

	}
        /* switch to next state */
        state=LDCS_STATE_DONE;
        break;
      }

      /* only filename --> no result  */
    case LDCS_STATE_CLIENT_PROCESS_REJECTED_QUERY:
      {
        ldcs_message_t out_msg;
        int connid;
        out_msg.header.type=LDCS_MSG_FILE_QUERY_ANSWER;
        out_msg.alloclen=MAX_PATH_LEN;  out_msg.data=buffer_out;

        connid=ldcs_process_data->client_table[nc].connid;
        out_msg.data[0]='\0'; out_msg.header.len=0;
      
	/* send answer only to active client not to pseudo client */
	if ( (ldcs_process_data->client_table[nc].state==LDCS_CLIENT_STATUS_ACTIVE) && 
	     (connid>=0) )  {

	  ldcs_send_msg(connid,&out_msg);
	  ldcs_process_data->client_table[nc].query_open=0;
	  log_msg("SERVER[%03d][C%02d]: answer query (rejected): '%s'\n", ldcs_process_data->md_rank, nc, out_msg.data);

	  /* statistic */
	  ldcs_process_data->server_stat.clientmsg.cnt++;
	  ldcs_process_data->server_stat.clientmsg.time+=(ldcs_get_time()-
							  ldcs_process_data->client_table[nc].query_arrival_time);

	}
        /* switch to next state */
        state=LDCS_STATE_DONE;
        break;
      }

      /* only filename --> no result  */
    case LDCS_STATE_CLIENT_PROCESS_NODIR_QUERY:
      {
        ldcs_message_t out_msg;
        int connid;

        out_msg.header.type=LDCS_MSG_FILE_QUERY_ANSWER;
        out_msg.alloclen=MAX_PATH_LEN;  out_msg.data=buffer_out;

        connid=ldcs_process_data->client_table[nc].connid;
        out_msg.header.type=LDCS_MSG_FILE_QUERY_ANSWER;
        strncpy(out_msg.data,ldcs_process_data->client_table[nc].query_filename,MAX_PATH_LEN);  
        out_msg.header.len=strlen(ldcs_process_data->client_table[nc].query_filename); 

	/* send answer only to active client not to pseudo client */
	if ( (ldcs_process_data->client_table[nc].state==LDCS_CLIENT_STATUS_ACTIVE) && 
	     (connid>=0) )  {
	  ldcs_send_msg(connid,&out_msg);
	  ldcs_process_data->client_table[nc].query_open=0;
	  log_msg("SERVER[%03d][C%02d]: answer query (nodir): '%s'\n", ldcs_process_data->md_rank, nc, out_msg.data);

	  /* statistic */
	  ldcs_process_data->server_stat.clientmsg.cnt++;
	  ldcs_process_data->server_stat.clientmsg.time+=(ldcs_get_time()-
							  ldcs_process_data->client_table[nc].query_arrival_time);

	}
        /* switch to next state */
        state=LDCS_STATE_DONE;
        break;
      }

      /* END Message from Client received */
    case LDCS_STATE_CLIENT_END_MSG:
      {
        int connid = ldcs_process_data->client_table[nc].connid;
        log_msg("SERVER[%03d][C%02d]: recvd END: closing connection\n", ldcs_process_data->md_rank, nc);
        printf("SERVER[%02d]: close client connection on host %s #c=%02d at %12.4f\n", ldcs_process_data->md_rank, 
               ldcs_process_data->hostname, 
               ldcs_process_data->client_counter,ldcs_get_time());

	if ( (ldcs_process_data->client_table[nc].state==LDCS_CLIENT_STATUS_ACTIVE) && 
	     (connid>=0) )  {
	  
	  ldcs_listen_unregister_fd(ldcs_get_fd(connid)); 
	  ldcs_process_data->client_table[nc].state = LDCS_CLIENT_STATUS_FREE;
	  ldcs_process_data->client_table_used--;
	  ldcs_close_server_connection(connid);
	}
	state=LDCS_STATE_DONE;
        break;
      }
    
      /* ****************************************** */
      /* UPDATE ACTIONS                             */
      /* ****************************************** */


    case LDCS_STATE_CLIENT_START_UPDATE:
      {
        /* check all clients with open queries */
        ldcs_process_data->update=LDCS_UPDATE_STATE_ONGOING;
        ldcs_process_data->update_next_nc=0;
        
        state=LDCS_STATE_CLIENT_UPDATE;
        break;
      }

    case LDCS_STATE_CLIENT_UPDATE:
      {
        /* check all clients with open queries */
        int found=0;
        while((ldcs_process_data->update_next_nc<ldcs_process_data->client_table_used) && (found==0)) {
          if(ldcs_process_data->client_table[nc].query_open) {
            ldcs_process_data->last_action_on_nc=nc;
            found=1;
          }
          ldcs_process_data->update_next_nc++;
        }

        if(!found) {
          ldcs_process_data->update=LDCS_UPDATE_STATE_NONE;
        }

        /* switch to next state */
        if(found) {
          state=LDCS_STATE_CLIENT_FILE_QUERY_CHECK;
        } else {
          state=LDCS_STATE_READY;
        }

        break;
      }

    case LDCS_STATE_DONE:
      {
        
        state=LDCS_STATE_READY;

        /* check if other action are required */
        if(ldcs_process_data->update==LDCS_UPDATE_STATE_INITIATE) {
          state=LDCS_STATE_CLIENT_START_UPDATE;
        }

        if(ldcs_process_data->update==LDCS_UPDATE_STATE_ONGOING) {
          state=LDCS_STATE_CLIENT_UPDATE;
        }

        break;
      }
    
    default: ;
      debug_printf("STATE[%03d]: unknown state: nc=%d %s ...\n", count,nc,
                   _state_type_to_str(state) );
    }
  } /* while(state...) */
  return(state);
}

    char* _state_type_to_str (ldcs_state_t state) {

      return(
             (state ==   LDCS_STATE_READY                         )? "LDCS_STATE_READY":                              
             (state ==   LDCS_STATE_DONE                              )? "LDCS_STATE_DONE":                                 
             (state ==   LDCS_STATE_CLIENT_INFO_MSG                   )? "LDCS_STATE_CLIENT_INFO_MSG":              
             (state ==   LDCS_STATE_CLIENT_END_MSG            )? "LDCS_STATE_CLIENT_END_MSG":               
             (state ==   LDCS_STATE_CLIENT_FILE_QUERY_MSG             )? "LDCS_STATE_CLIENT_FILE_QUERY_MSG":                
             (state ==   LDCS_STATE_CLIENT_FILE_QUERY_EXACT_PATH_MSG )? "LDCS_STATE_CLIENT_FILE_QUERY_EXACT_PATH_MSG":              
             (state ==   LDCS_STATE_CLIENT_FILE_QUERY_CHECK           )? "LDCS_STATE_CLIENT_FILE_QUERY_CHECK":      
             (state ==   LDCS_STATE_CLIENT_FORWARD_QUERY              )? "LDCS_STATE_CLIENT_FORWARD_QUERY":                 
             (state ==   LDCS_STATE_CLIENT_PROCESS_FILE       )? "LDCS_STATE_CLIENT_PROCESS_FILE":                  
             (state ==   LDCS_STATE_CLIENT_PROCESS_DIR        )? "LDCS_STATE_CLIENT_PROCESS_DIR":                   
             (state ==   LDCS_STATE_CLIENT_PROCESS_LOCAL_FILE_QUERY )? "LDCS_STATE_CLIENT_PROCESS_LOCAL_FILE_QUERY":    
             (state ==   LDCS_STATE_CLIENT_PROCESS_REJECTED_QUERY     )? "LDCS_STATE_CLIENT_PROCESS_REJECTED_QUERY":        
             (state ==   LDCS_STATE_CLIENT_PROCESS_FULFILLED_QUERY  )? "LDCS_STATE_CLIENT_PROCESS_FULFILLED_QUERY":         
             (state ==   LDCS_STATE_CLIENT_PROCESS_NODIR_QUERY      )? "LDCS_STATE_CLIENT_PROCESS_NODIR_QUERY":     
             (state ==   LDCS_STATE_CLIENT_MYRANKINFO_MSG      )? "LDCS_STATE_CLIENT_MYRANKINFO_MSG":     
             (state ==   LDCS_STATE_CLIENT_START_UPDATE               )? "LDCS_STATE_CLIENT_START_UPDATE":                  
             (state ==   LDCS_STATE_CLIENT_UPDATE             )? "LDCS_STATE_CLIENT_UPDATE":                
             (state ==   LDCS_STATE_UNKNOWN                       )? "LDCS_STATE_UNKNOWN":                             
             "???");
    }
