#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 

#include "ldcs_api.h" 

#define MSG_SIZE 256

void error(const char *msg)
{
    perror(msg);
    exit(0);
}

int main(int argc, char *argv[])
{
  int cid, n;
  
  ldcs_message_t message;
  ldcs_message_t end_message;
  
  char buffer[256];
  char *result;

  if (argc < 3) {
    fprintf(stderr,"usage %s hostname port msg\n", argv[0]);
    exit(0);
  }

  cid=ldcs_open_connection(argv[1],atoi(argv[2]));

  /* query */
  message.header.type=LDCS_MSG_UNKNOWN;
  message.data=(char*) malloc(MSG_SIZE);

  if(!message.data) error("could not allocate memory for message"); 

  strcpy(message.data,"/usr/lib");message.header.len=strlen(message.data)+1;
  ldcs_send_msg(cid,&message);

  strcpy(message.data,"/usr/local/lib");message.header.len=strlen(message.data)+1;
  ldcs_send_msg(cid,&message);

  strcpy(buffer,"/lib/libc.so");
  ldcs_send_FILE_QUERY(cid,"librt.so.1",&result);  printf("FILE_QUERY: %s -> %s (after)\n",buffer,result);
  ldcs_send_FILE_QUERY(cid,"/usr/lib/mpi/gcc/openmpi/lib/librt.so.1",&result);  printf("FILE_QUERY: %s -> %s (after)\n",buffer,result);
  ldcs_send_FILE_QUERY(cid,"./tls/i686/sse2/librt.so.1",&result);  printf("FILE_QUERY: %s -> %s (after)\n",buffer,result);
  ldcs_send_FILE_QUERY(cid,"./tls/i686/librt.so.1",&result);  printf("FILE_QUERY: %s -> %s (after)\n",buffer,result);
  ldcs_send_FILE_QUERY(cid,"./tls/sse2/librt.so.1" ,&result);  printf("FILE_QUERY: %s -> %s (after)\n",buffer,result);
  ldcs_send_FILE_QUERY(cid,"./tls/librt.so.1" ,&result);  printf("FILE_QUERY: %s -> %s (after)\n",buffer,result);
  ldcs_send_FILE_QUERY(cid,"./i686/sse2/librt.so.1" ,&result);  printf("FILE_QUERY: %s -> %s (after)\n",buffer,result);
  ldcs_send_FILE_QUERY(cid,"./i686/librt.so.1" ,&result);  printf("FILE_QUERY: %s -> %s (after)\n",buffer,result);
  ldcs_send_FILE_QUERY(cid,"./sse2/librt.so.1" ,&result);  printf("FILE_QUERY: %s -> %s (after)\n",buffer,result);
  ldcs_send_FILE_QUERY(cid,"./librt.so.1",&result);  printf("FILE_QUERY: %s -> %s (after)\n",buffer,result);
  ldcs_send_FILE_QUERY(cid,"/home/zam/zdv087/TRAC/wfrings/promotion/LLNL_LDSO/LDCS/auditclient/librt.so.1" ,&result);  printf("FILE_QUERY: %s -> %s (after)\n",buffer,result);

  end_message.header.type=LDCS_MSG_END;  end_message.header.len=0;end_message.data=NULL;
  ldcs_send_msg(cid,&end_message);


  ldcs_close_connection(cid);
  return 0;
}


