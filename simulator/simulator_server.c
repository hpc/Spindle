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
#include "ldcs_cache.h" 
#include "ldcs_audit_server_process.h"
#include "ldcs_audit_server_md.h"

#include "simulator_server.h" 


#define MSG_SIZE 256

int _ready_cb_func (  void * data) {
  int rc=0;
  printf("SERVER: starting Barrier after server ready\n");
  MPI_Barrier(MPI_COMM_WORLD);

  return(rc);
}

int simulator_server ( MPI_Comm mycomm,
		       char  *location, int locmod, int number ) {
  int rc=0;
  char helpstr[32];
  int rank, size;
  MPI_Comm_rank(mycomm,&rank);
  MPI_Comm_size(mycomm,&size);

  
  debug_printf3("startup server (%s, %d)\n",location,number);
  printf("SERVER: startup server (%s, %d)\n",location,number);
  
  sprintf(helpstr,"%d",locmod);
  setenv("LDCS_LOCATION_MOD",helpstr,1);

  sprintf(helpstr,"%d",20);
  setenv("LDCS_NPORTS",helpstr,1);

  sprintf(helpstr,"%d",1);
  setenv("LDCS_EXIT_AFTER_SESSION",helpstr,1);

  ldcs_audit_server_process(location,number,&_ready_cb_func, NULL);

  /* dump hash */
  if(1) {
    char helpstr[MAX_PATH_LEN];
    sprintf(helpstr,"_cache_%d_of_%d.dat",rank+1,size);
    ldcs_cache_dump(helpstr);

  }

  
  debug_printf3("shutdown server (%s, %d)\n",location,number);
  printf("SERVER: shutdown server (%s, %d)\n",location,number);


  return(rc);
}
