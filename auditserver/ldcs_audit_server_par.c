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
#include "ldcs_audit_server_process.h" 


void error(const char *msg)
{
    perror(msg);
    exit(1);
}

int _ready_cb_func (  void * data) {
  int rc=0;
  return(rc);
}

int main(int argc, char *argv[])
{
  int number;
  char *location;
 
  

  if (argc < 3) {
    fprintf(stderr,"no location and number provided, use ENVs LDCS_LOCATION LDCS_NUMBER\n");
    location = getenv("LDCS_LOCATION");
    number   = atoi(getenv("LDCS_NUMBER"));
  } else {
    location = argv[1];
    number   = atoi(argv[3]);
  } 

  debug_printf3("startup server (%s, %d)\n",location,number);
  printf("SERVER: startup server (%s, %d)\n",location,number);

  ldcs_audit_server_process(location,number,&_ready_cb_func, NULL);

  debug_printf3("shutdown server (%s, %d)\n",location,number);
  printf("SERVER: shutdown server (%s, %d)\n",location,number);

  /* needed this sleep so that server prints out all debug info (don't know why yet) */
  sleep(1);

  return 0; 
}
