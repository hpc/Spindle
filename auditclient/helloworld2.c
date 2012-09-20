#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <unistd.h>

int fooA ();
int barA ();

int main (int argc, char **argv)
{

  /* struct hostent *server; */
  int fd;

  printf("Hello World\n");

  
  /* server = gethostbyname("localhost");  */
  /* printf("helloworld2: after gethostbyname: -> server=%x\n",server); */

  fd=open("/etc/hosts",O_RDONLY);
  close(fd);

  fooA();

  barA();

 
  exit(0);
}
