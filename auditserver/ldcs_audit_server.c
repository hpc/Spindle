/* A simple server in the internet domain using TCP
   The port number is passed as an argument */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>

#include "ldcs_api.h" 
#include "ldcs_cache.h" 
#include "ldcs_hash.h" 

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[])
{
  int serverid, connid , number;
  char *location, *newfn;
  int end_message_received, null_msg_cnt;
 
  ldcs_message_t in_msg,out_msg;
  char buffer_in[MAX_PATH_LEN];
  char buffer_out[MAX_PATH_LEN];

  char remote_cwd[MAX_PATH_LEN];

  if (argc < 4) {
    err_printf(stderr,"no location and number provided, use ENVs LDCS_LOCATION LDCS_NUMBER\n");
    location = getenv("LDCS_LOCATION");
    number   = atoi(getenv("LDCS_NUMBER"));
  } else {
    location = argv[1];
    number   = atoi(argv[3]);
  } 

  debug_printf("Create server (%s, %d)\n",location,number);

  serverid = ldcs_create_server(location,number);

  ldcs_msg_init(&in_msg);
  in_msg.header.type=LDCS_MSG_UNKNOWN;
  in_msg.alloclen=MAX_PATH_LEN;
  in_msg.header.len=0;
  in_msg.data=buffer_in;

  ldcs_msg_init(&out_msg);
  out_msg.header.type=LDCS_MSG_UNKNOWN;
  out_msg.alloclen=MAX_PATH_LEN;
  out_msg.header.len=0;
  out_msg.data=buffer_out;

  while(1) {
    connid = ldcs_open_server_connection(serverid);

    end_message_received=0;

    null_msg_cnt=0;    
    while((!end_message_received) && (null_msg_cnt<2) ) {

      ldcs_recv_msg_static(connid,&in_msg, LDCS_READ_NO_BLOCK);
      
      /* prevent infinite loop */
      if(in_msg.header.len==0) {
         debug_printf3("null_msg_cnt = %d\n",null_msg_cnt);
         null_msg_cnt++;
      } else {
         null_msg_cnt=0;
      }
        

      switch(in_msg.header.type) {
      case LDCS_MSG_END:
        debug_printf2("End message received\n");
        end_message_received=1;
        break;

      case LDCS_MSG_CWD:
        strncpy(remote_cwd,in_msg.data,MAX_PATH_LEN);
        debug_printf2("cwd message received -> %s\n",remote_cwd);
        break;
        
      case LDCS_MSG_FILE_QUERY:
        debug_printf2("file query message received\n");
        out_msg.header.type=LDCS_MSG_FILE_QUERY_ANSWER;
        out_msg.data[0]='\0';
        newfn=findDirForFile(in_msg.data,remote_cwd);
        if(newfn) {
          strncpy(out_msg.data,newfn,MAX_PATH_LEN);          
          out_msg.header.len=strlen(newfn);
          free(newfn);
        } else {
          out_msg.header.len=0;
        }
        ldcs_send_msg(connid,&out_msg);
 
        break;
      default:
         err_printf("recvd unknown message of type: %s len=%d data=%s ...\n",
                    _message_type_to_str(in_msg.header.type),
                    in_msg.header.len, in_msg.data );
         break;
      }

    }
    ldcs_cache_dump("./hash.dump");
    debug_printf3("close connection %d\n",connid);
    ldcs_close_server_connection(connid);
  }

  debug_printf3("destroy server (%s,%d)\n",location,number);
  ldcs_destroy_server(serverid);
 

  return 0; 
}
