#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "spindle_debug.h"
#include "config.h"

#if !defined(LIBDIR)
#error Expected to be built with libdir defined
#endif

#include "ldcs_api.h"

static int rankinfo[4]={-1,-1,-1,-1};
static int ldcs_id;
static int number;
static char *location, *number_s;
static char **cmdline;
static char *executable;
static char *client_lib;

char libstr_socket[] = LIBDIR "/libspindle_client_socket.so";
char libstr_pipe[] = LIBDIR "/libspindle_client_pipe.so";

#if defined(COMM_SOCKET)
static char *default_libstr = libstr_socket;
#elif defined(COMM_PIPES)
static char *default_libstr = libstr_pipe;
#else
#error Unknown connection type
#endif

static int establish_connection()
{
   debug_printf2("Opening connection to server\n");
   ldcs_id = ldcs_open_connection(location, number);
   if (ldcs_id == -1) 
      return -1;

   ldcs_send_CWD(ldcs_id);
   ldcs_send_HOSTNAME(ldcs_id);
   ldcs_send_PID(ldcs_id);
   ldcs_send_LOCATION(ldcs_id, location);
   ldcs_send_MYRANKINFO_QUERY(ldcs_id, &rankinfo[0], &rankinfo[1], &rankinfo[2], &rankinfo[3]);      

   return 0;
}

static void setup_environment()
{
   char rankinfo_str[256];
   snprintf(rankinfo_str, 256, "%d %d %d %d %d", ldcs_id, rankinfo[0], rankinfo[1], rankinfo[2], rankinfo[3]);

   char *connection_str = ldcs_get_connection_string(ldcs_id);
   assert(connection_str);

   setenv("LD_AUDIT", client_lib, 1);
   setenv("LDCS_LOCATION", location, 1);
   setenv("LDCS_NUMBER", number_s, 1);
   setenv("LDCS_RANKINFO", rankinfo_str, 1);
   setenv("LDCS_CONNECTION", connection_str, 1);
}

static int parse_cmdline(int argc, char *argv[])
{
   if (argc < 3)
      return -1;

   location = argv[1];
   number_s = argv[2];
   number = atoi(number_s);
   cmdline = argv + 3;

   return 0;
}

static void get_executable()
{
   ldcs_send_FILE_QUERY_EXACT_PATH(ldcs_id, *cmdline, &executable);
   if (executable == NULL) {
      executable = *cmdline;
      err_printf("Failed to relocate executable %s\n", executable);
   }
   else {
      debug_printf("Relocated executable %s to %s\n", *cmdline, executable);
      chmod(executable, 0700);
   }
}

static void get_clientlib()
{
   ldcs_send_FILE_QUERY_EXACT_PATH(ldcs_id, default_libstr, &client_lib);
   if (client_lib == NULL) {
      client_lib = default_libstr;
      err_printf("Failed to relocate client library %s\n", default_libstr);
   }
   else {
      debug_printf("Relocated client library %s to %s\n", default_libstr, client_lib);
      chmod(client_lib, 0600);
   }
}

int main(int argc, char *argv[])
{
   int error, i, result;

   LOGGING_INIT_PREEXEC("Client");
   debug_printf("Launched Spindle Bootstrapper\n");

   result = parse_cmdline(argc, argv);
   if (result == -1) {
      fprintf(stderr, "spindle_boostrap cannot be invoked directly\n");
      return -1;
   }

   result = establish_connection();
   if (result == -1) {
      err_printf("spindle_bootstrap failed to connect to daemons\n");
      return -1;
   }

   get_executable();
   get_clientlib();

   /**
    * Exec setup
    **/
   debug_printf("Spindle bootstrap launching: ");
   if (argc == 3) {
      bare_printf("<no executable given>");
   }
   else {
      bare_printf("%s ", executable);
      for (i=4; i<argc; i++) {
         bare_printf("%s ", argv[i]);
      }
   }
   bare_printf("\n");

   /**
    * Exec the user's application.
    **/
   setup_environment();
   execv(executable, cmdline);

   /**
    * Exec error handling
    **/
   error = errno;
   err_printf("Error execing app: %s\n", strerror(errno));

   return -1;
}
