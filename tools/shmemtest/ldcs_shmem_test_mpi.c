#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h> 

#include <errno.h>

#include <ldcs_api.h>
#include <ldcs_api_shmem.h>
#include <mpi.h>


void error(const char *msg)
{
    perror(msg);
    exit(0);
}

int main(int argc, char *argv[])
{
  int n;
  
  char buffer[256];
  char *result;
  int rank,size;
  char hostname[100];

  int ldcsid=-1, iamserver=-1;
  int number=-1;

  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD,&rank);
  MPI_Comm_size(MPI_COMM_WORLD,&size);

  gethostname(hostname,100);

  char* location =getenv("LDCS_LOCATION");
  char* locmodstr=getenv("LDCS_LOCATION_MOD");
  char* numberstr=getenv("LDCS_NUMBER");
  char  mylocation[MAX_PATH_LEN];
  if(numberstr>0) {
    number=atoi(numberstr);
  }

  if(locmodstr>0) {
    int locmod=atoi(locmodstr);
    debug_printf3("multiple server per node add modifier to location mod=%d\n",locmod);
    if(strlen(location)+10<MAX_PATH_LEN) {
      sprintf(mylocation,"%s-%02d",location,rank%locmod);
      debug_printf3("change location to %s (locmod=%d)\n",mylocation,locmod);
    } else error("location path too long");
  } else {
    strncpy(mylocation,location,MAX_PATH_LEN);
  }

  ldcsid=ldcs_create_server_or_client_shmem(mylocation,number);

  iamserver=ldcs_is_server_shmem(ldcsid);

  printf("Hello World: %d of %d start PID=%d HOSTNAME=%s %s\n",rank,size,getpid(),hostname,(iamserver)?"SERVER":"CLIENT");

#if 0
  if(ldcsid==-1) {
    error("could not open connection");
  }
#endif
  
  MPI_Barrier(MPI_COMM_WORLD);
  
  printf("Hello World: %d of %d finished\n",rank,size);
  
  MPI_Finalize();
  
  return 0;
}


