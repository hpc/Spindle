/*
This file is part of Spindle.  For copyright information see the COPYRIGHT 
file in the top level directory, or at 
https://github.com/hpc/Spindle/blob/master/COPYRIGHT

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License (as published by the Free Software
Foundation) version 2.1 dated February 1999.  This program is distributed in the
hope that it will be useful, but WITHOUT ANY WARRANTY; without even the IMPLIED
WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms 
and conditions of the GNU General Public License for more details.  You should 
have received a copy of the GNU Lesser General Public License along with this 
program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include <lmon_api/common.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <lmon_api/common.h>
#include <lmon_api/lmon_proctab.h>
#include <lmon_api/lmon_be.h>

#include "config.h"
#include "spindle_usrdata.h"
#include "ldcs_api_opts.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "ldcs_audit_server_md.h"
#include "ldcs_api.h"
#ifdef __cplusplus
}
#endif

static int rank, size;
unsigned long opts;
unsigned int shared_secret;
static char *pythonprefix;

int _ready_cb_func (  void * data) {
  int rc=0;
  MPIR_PROCDESC_EXT *proctab;
  int proctab_size;
  lmon_rc_e lrc;
  int signum, i;

#if defined(os_bg)
  signum = 0;
#elif defined(os_linux)
  signum = SIGCONT;
#else
#error Unknown OS
#endif

  lrc = LMON_be_getMyProctabSize(&proctab_size);
  if (lrc != LMON_OK)
  {
     err_printf("LMON_be_getMyProctabSize\n");
     LMON_be_finalize();
     return EXIT_FAILURE;
  }
  
  proctab = (MPIR_PROCDESC_EXT *) malloc (proctab_size*sizeof(MPIR_PROCDESC_EXT));
  if ( proctab == NULL )  {
     err_printf("Proctable malloc return null\n");
     LMON_be_finalize();
     return EXIT_FAILURE;
  }

  lrc = LMON_be_getMyProctab(proctab, &proctab_size, proctab_size);
  if (lrc != LMON_OK)   {    
     err_printf("LMON_be_getMyProctab\n");
     LMON_be_finalize();
     return EXIT_FAILURE;
  }

  /* Continue application tasks */
  for(i=0; i < proctab_size; i++)  {
    debug_printf3("[LMON BE] kill %d, %d\n", proctab[i].pd.pid, signum );
    kill(proctab[i].pd.pid, signum);
  }
  
  for (i=0; i < proctab_size; i++) {
    if (proctab[i].pd.executable_name) free(proctab[i].pd.executable_name);
    if (proctab[i].pd.host_name)       free(proctab[i].pd.host_name);
  }
  free (proctab);

  return(rc);
}

extern "C" {
   char *parse_location(char *loc);
}

int main(int argc, char* argv[])
{
  lmon_rc_e lrc;
  int number;
  unsigned int port;
  char *location;
  spindle_daemon_args dargs;

  LOGGING_INIT(const_cast<char *>("Server"));
  if (spindle_debug_output_f) {
     int fd = fileno(spindle_debug_output_f);
     close(1);
     close(2);
     dup2(fd, 1);
     dup2(fd, 2);
     fprintf(stdout, "Test-stdout\n");
     fprintf(stderr, "Test-stderr\n");
  }

  debug_printf("Spindle Server Cmdline: ");
  for (int i=0; i<argc; i++) {
     bare_printf("%s ", argv[i]);
  }
  bare_printf("\n");

  /* Initialization of LMON  */
  lrc = LMON_be_init(LMON_VERSION, &argc, &argv);
  if (lrc != LMON_OK) {      
     err_printf("Failed LMON_be_init\n");
     return EXIT_FAILURE;
  }

  /* Register pack/unpack functions to LMON  */
  if ( LMON_be_amIMaster() == LMON_YES ) {
    
    if ( ( lrc = LMON_be_regUnpackForFeToBe ( unpackfebe_cb )) != LMON_OK ) {
       err_printf("Failed LMON_be_regUnpackForBeToFe\n");
       return EXIT_FAILURE;
    } 
  }
  
  LMON_be_getMyRank(&rank);
  LMON_be_getSize(&size);
  debug_printf("Launchmon rank %d/%d\n", rank, size);

  lrc = LMON_be_handshake(NULL);
  if (lrc != LMON_OK)
  {
     err_printf("Failed LMON_be_handhshake\n");
     LMON_be_finalize();
     return EXIT_FAILURE;
  }

  lrc = LMON_be_ready(NULL);
  if (lrc != LMON_OK)
  {     
     err_printf("Failed LMON_be_ready\n");
     LMON_be_finalize();
     return EXIT_FAILURE;
  } 

  /* Recieve user data on master */
  lrc = LMON_be_recvUsrData(&dargs);
  if (lrc != LMON_OK) {
     err_printf("Failed to receive usr data from FE\n");
     return EXIT_FAILURE;
  }

  /* Broadcast data from master */
  int actual_size = 0;
  void *buffer;
  if (LMON_be_amIMaster() == LMON_YES) {
     int estimate_size = sizeof(dargs) + 
        strlen(dargs.location) + 1 +
        strlen(dargs.pythonprefix) + 1;
     buffer = (void *) malloc(estimate_size);
     packfebe_cb(&dargs, buffer, estimate_size, &actual_size);
     LMON_be_broadcast(&actual_size, sizeof(actual_size));
     LMON_be_broadcast(buffer, actual_size);
  }
  else {
     LMON_be_broadcast(&actual_size, sizeof(actual_size));
     buffer = (void *) malloc(actual_size);
     LMON_be_broadcast(buffer, actual_size);
     unpackfebe_cb(buffer, actual_size, &dargs);
  }
  free(buffer);

  number = dargs.number;
  port = dargs.port;
  opts = dargs.opts;
  shared_secret = dargs.shared_secret;
  location = dargs.location;
  pythonprefix = dargs.pythonprefix;

  location = parse_location(location);
  if (!location) {
     LMON_be_finalize();
     return -1;
  }

  /* start SPINDLE server */
  ldcs_audit_server_process(location, port, number, pythonprefix, &_ready_cb_func, NULL);

  LMON_be_finalize();

  debug_printf("Finished server process.  Exiting with success\n");
  
  LOGGING_FINI;

  return EXIT_SUCCESS;
}
