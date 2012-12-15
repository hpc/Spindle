#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <signal.h>

#include "config.h"

#if !defined(LIBDIR)
#error Expected to be built with libdir defined
#endif

char libstr_socket[] = LIBDIR "/libldcs_audit_client_socket.so";
char libstr_pipe[] = LIBDIR "/libldcs_audit_client_pipe.so";

#if defined(COMM_SOCKET)
char *default_libstr = libstr_socket;
#elif defined(COMM_PIPES)
char *default_libstr = libstr_pipe;
#else
#error Unknown connection type
#endif

int main(int argc, char *argv[])
{
   int error, i;

   /* TODO: Start boostrapping executable */

   printf("At top of Spindle Bootstrap\n");

   /**
    * Setup LD_AUDIT in target program
    **/
   assert(argc >= 3);
   setenv("LD_AUDIT", default_libstr, 1);
   setenv("LDCS_LOCATION", argv[1], 1);
   setenv("LDCS_NUMBER", argv[2], 1);

   fprintf(stderr, "In app process.  libstr_pipe = %s, location = %s, number = %s\n",
           libstr_pipe, argv[1], argv[2]);


   /**
    * Pause ourselves to sync up with the daemon.  We'll get continued when 
    * it's ready for us.
    **/
   kill(getpid(), SIGSTOP);

   /**
    * Exec the user's application.
    **/
   execv(argv[3], argv+3);

   /**
    * Exec error handling
    **/
   error = errno;
   fprintf(stderr, "Spindle BE error launching: ");
   if (argc == 3) {
      fprintf(stderr, "<no executable given>");
   }
   else {
      for (i=3; i<argc; i++) {
         fprintf(stderr, "%s ", argv[i]);
      }
   }
   fprintf(stderr, ": %s\n", strerror(error));
   return -1;
}
