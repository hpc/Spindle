#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>

#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 


#include <mpi.h>

int main (int argc, char **argv)
{

  int rank,size;
  char hostname[100];

  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD,&rank);
  MPI_Comm_size(MPI_COMM_WORLD,&size);
  gethostname(hostname,100);
  printf("Hello World: %d of %d start PID=%d HOSTNAME=%s\n",rank,size,getpid(),hostname);

 
  MPI_Barrier(MPI_COMM_WORLD);
  
  /* sleep(30);  */

  MPI_Barrier(MPI_COMM_WORLD);

  printf("Hello World: %d of %d finish, ... sleep(3) after MPI_Finalize ...  \n",rank,size);
  MPI_Finalize();

  /* sleep(3);  */
  printf("Hello World: %d of %d finished,\n",rank,size);

  exit(0);
}
