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

int main (int argc, char **argv)
{

  void *handle;
  int (*fooB_ptr)();
  char *error;

  printf("Hello World\n");

  fooA();

  barA();

  handle = dlopen ("libsampleB.so", RTLD_LAZY);
  if (!handle) {
    fprintf (stderr, "%s\n", dlerror());
    exit(1);
  }

  dlerror();    /* Clear any existing error */
  fooB_ptr = dlsym(handle, "fooB");
  if ((error = dlerror()) != NULL)  {
    fprintf (stderr, "%s\n", error);
    exit(1);
  }
  (*fooB_ptr)();
  
  dlclose(handle);
  return 0;


  sleep(5);
 
  exit(0);
}
