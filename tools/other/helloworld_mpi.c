#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>

#include <mpi.h>

int main (int argc, char **argv)
{

  void *handle;
  int (*fooB_ptr)();
  char *error;
  int rank,size;
  char hostname[100];

  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD,&rank);
  MPI_Comm_size(MPI_COMM_WORLD,&size);

  gethostname(hostname,100);

  /* if((rank==0) || (rank==size-1)) { */
    printf("Hello World: %d of %d start PID=%d HOSTNAME=%s\n",rank,size,getpid(),hostname);
  /* } */

  MPI_Barrier(MPI_COMM_WORLD);


  MPI_Finalize();

  exit(0);
}
