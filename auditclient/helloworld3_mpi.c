#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 

#include <dlfcn.h>

#include <mpi.h>

#include <ldcs_api.h>
#define USEDL

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
  printf("Hello World: %d of %d start PID=%d HOSTNAME=%s\n",rank,size,getpid(),hostname);

  fooA();

  barA();
  
  MPI_Barrier(MPI_COMM_WORLD);

#ifdef USEDL
  handle = dlopen ("libsampleB.la", RTLD_LAZY);
  if (!handle) {
    fprintf (stderr, "could not open libsampleB.so on rank %d %s\n", rank, dlerror);
    fprintf (stderr, "%s\n", dlerror());
    exit(1);
  }

  dlerror();    /* Clear any existing error */
  fooB_ptr = dlsym(handle, "fooB");
  if ((error = dlerror()) != NULL)  {
    fprintf (stderr, "could not load fooB on rank %d %s\n", rank, error);
  } else {
    (*fooB_ptr)();
  }
  dlclose(handle);
#endif

  MPI_Barrier(MPI_COMM_WORLD);

  printf("Hello World: %d of %d finish, ... sleep(3) after MPI_Finalize ...  \n",rank,size);
  MPI_Finalize();

  /* TDB: check why needed */
  sleep(3); 
  printf("Hello World: %d of %d finished,\n",rank,size);

  exit(0);
}
