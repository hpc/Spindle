#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 

#include <mpi.h>

#include "ldcs_api.h" 

#define MSG_SIZE 256

void error(const char *msg)
{
    perror(msg);
    exit(0);
}

int main(int argc, char *argv[])
{
  int ldcsid;
  
  ldcs_message_t message;
  ldcs_message_t end_message;
  
  char buffer[256];
  char *result;
  int rank,size;
  char hostname[100];

  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD,&rank);
  MPI_Comm_size(MPI_COMM_WORLD,&size);
  gethostname(hostname,100);
  printf("Hello World: %d of %d start PID=%d HOSTNAME=%s\n",rank,size,getpid(),hostname);

  char* ldcs_location=getenv("LDCS_LOCATION");
  int   ldcs_number  =atoi(getenv("LDCS_NUMBER"));
  char* ldcs_locmodstr=getenv("LDCS_LOCATION_MOD");
  
  if(ldcs_locmodstr) {
    int ldcs_locmod=atoi(ldcs_locmodstr);
    char buffer[MAX_PATH_LEN];
    debug_printf("multiple server per node add modifier to location mod=%d\n",ldcs_locmod);
    if(strlen(ldcs_location)+10<MAX_PATH_LEN) {
      sprintf(buffer,"%s-%02d",ldcs_location,getpid()%ldcs_locmod);
      debug_printf("open connection to ldcs %s %d\n",buffer,ldcs_number);
      ldcsid=ldcs_open_connection(buffer,ldcs_number);
    } else _error("location path too long");
  } else {
    debug_printf("open connection to ldcs %s %d\n",ldcs_location,ldcs_number);
    ldcsid=ldcs_open_connection(ldcs_location,ldcs_number);
  }
  if(ldcsid>-1) {
    ldcs_send_CWD(ldcsid);
    ldcs_send_HOSTNAME(ldcsid);
    ldcs_send_PID(ldcsid);
  }

  /* query */
  message.header.type=LDCS_MSG_UNKNOWN;
  message.data=(char*) malloc(MSG_SIZE);

  if(!message.data) error("could not allocate memory for message"); 

  strcpy(message.data,"/usr/lib");message.header.len=strlen(message.data)+1;
  ldcs_send_msg(ldcsid,&message);

  strcpy(message.data,"/usr/local/lib");message.header.len=strlen(message.data)+1;
  ldcs_send_msg(ldcsid,&message);

  strcpy(buffer,"/lib/libc.so");
  ldcs_send_FILE_QUERY(ldcsid,"librt.so.1",&result);  printf("FILE_QUERY: %s -> %s (after)\n",buffer,result);
  ldcs_send_FILE_QUERY(ldcsid,"/usr/lib/mpi/gcc/openmpi/lib/librt.so.1",&result);  printf("FILE_QUERY: %s -> %s (after)\n",buffer,result);
  ldcs_send_FILE_QUERY(ldcsid,"./tls/i686/sse2/librt.so.1",&result);  printf("FILE_QUERY: %s -> %s (after)\n",buffer,result);
  ldcs_send_FILE_QUERY(ldcsid,"./tls/i686/librt.so.1",&result);  printf("FILE_QUERY: %s -> %s (after)\n",buffer,result);
  ldcs_send_FILE_QUERY(ldcsid,"./tls/sse2/librt.so.1" ,&result);  printf("FILE_QUERY: %s -> %s (after)\n",buffer,result);
  ldcs_send_FILE_QUERY(ldcsid,"./tls/librt.so.1" ,&result);  printf("FILE_QUERY: %s -> %s (after)\n",buffer,result);
  ldcs_send_FILE_QUERY(ldcsid,"./i686/sse2/librt.so.1" ,&result);  printf("FILE_QUERY: %s -> %s (after)\n",buffer,result);
  ldcs_send_FILE_QUERY(ldcsid,"./i686/librt.so.1" ,&result);  printf("FILE_QUERY: %s -> %s (after)\n",buffer,result);
  ldcs_send_FILE_QUERY(ldcsid,"./sse2/librt.so.1" ,&result);  printf("FILE_QUERY: %s -> %s (after)\n",buffer,result);
  ldcs_send_FILE_QUERY(ldcsid,"./librt.so.1",&result);  printf("FILE_QUERY: %s -> %s (after)\n",buffer,result);
  ldcs_send_FILE_QUERY(ldcsid,"/home/zam/zdv087/TRAC/wfrings/promotion/LLNL_LDSO/LDCS/auditclient/librt.so.1" ,&result);  printf("FILE_QUERY: %s -> %s (after)\n",buffer,result);

  end_message.header.type=LDCS_MSG_END;  end_message.header.len=0;end_message.data=NULL;
  ldcs_send_msg(ldcsid,&end_message);

  ldcs_close_connection(ldcsid);

  printf("Hello World: %d of %d finish, ... sleep(3) after MPI_Finalize ...  \n",rank,size);
  MPI_Finalize();

  /* TDB: check why needed */
  sleep(3); 
  printf("Hello World: %d of %d finished,\n",rank,size);

  return 0;
}


