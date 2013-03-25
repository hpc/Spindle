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

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

#if defined(DEBUG)
#define debug_printf3(format, ...) \
  do { \
     fprintf(stderr, "[%s:%u@%d] - " format, __FILE__, __LINE__, getpid(), ## __VA_ARGS__); \
  } while (0)
#else
#define debug_printf3(format, ...)
#endif

int main(int argc, char *argv[])
{
  int serverid, connid , number;
  char *location;
  int n, end_message_received, null_msg_cnt;
  ldcs_message_t* message_ptr;
  char help[41];

  if (argc < 3) {
    fprintf(stderr,"ERROR, no location and number provided\n");
    exit(1);
  }

  location = argv[1];
  number   = atoi(argv[2]);

  debug_printf3("create server (%s,%d)\n",location,number);

  serverid = ldcs_create_server(location,number);

  while(1) {
    connid = ldcs_open_server_connection(serverid);

    end_message_received=0;

    null_msg_cnt=0;    
    while((!end_message_received) && (null_msg_cnt<2) ) {

      message_ptr=ldcs_recv_msg(connid);
      
      bzero(help,41);if(message_ptr->data) strncpy(help,message_ptr->data,40);
      printf("SERVER: received message of type: %s len=%d data=%s ...\n",
	     _message_type_to_str(message_ptr->type),
	     message_ptr->len, help );

      /* prevent infinite loop */
      if(message_ptr->len==0) {
	debug_printf3("null_msg_cnt = %d\n",null_msg_cnt);
	null_msg_cnt++;
      } else {
	null_msg_cnt=0;
      }
	

      switch(message_ptr->type) {
      case LDCS_MSG_END:
	debug_printf3("end message received\n");
	end_message_received=1;
	break;
      default: ;
	break;
      }

      ldcs_msg_free(&message_ptr);
    }
    debug_printf3("close connection %d\n",connid);
    ldcs_close_server_connection(connid);
  }

  debug_printf3("destroy server (%s,%d)\n",location,number);
  ldcs_destroy_server(serverid);
 

  return 0; 
}
