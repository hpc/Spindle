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

#include "lmon_api/lmon_be.h"

#include "spindle_launch.h"
#include "spindle_debug.h"
#include "config.h"

#include <csignal>
#include <cassert>
#include <cstdlib>
#include <cstring>

static int unpackfebe_cb(void* udatabuf, 
                         int udatabuflen, 
                         void* udata) 
{
   spindle_args_t *args = (spindle_args_t *) udata;
   
   char *buffer = (char *) udatabuf;
   int pos = 0;

   memcpy(&args->port, buffer + pos, sizeof(args->port));
   pos += sizeof(args->port);

   memcpy(&args->num_ports, buffer + pos, sizeof(args->num_ports));
   pos += sizeof(args->num_ports);
   
   memcpy(&args->unique_id, buffer + pos, sizeof(args->unique_id));
   pos += sizeof(args->unique_id);

   assert(pos == udatabuflen);

   return 0;    
}

int releaseApplication(spindle_args_t *) {
  int rc=0;
  MPIR_PROCDESC_EXT *proctab;
  int proctab_size;
  lmon_rc_e lrc;
  int signum, i;

#if defined(os_bluegene)
  signum = 0;
#elif defined(os_linux)
  signum = SIGCONT;
#else
#error Unknown OS
#endif

  debug_printf("Sending SIGCONTs to each process to release debugger stops\n");
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

int startLaunchmonBE(int argc, char *argv[], int security_type)
{
   struct {
      unsigned int port;
      unsigned int num_ports;
      unique_id_t unique_id;
   } conn_info;
   lmon_rc_e rc;
   int result;

   /* Initialization of LMON  */
   rc = LMON_be_init(LMON_VERSION, &argc, &argv);
   if (rc != LMON_OK) {      
      err_printf("Failed LMON_be_init\n");
      return -1;
   }

   /* Register pack/unpack functions to LMON  */
   if (LMON_be_amIMaster() == LMON_YES) {
      rc = LMON_be_regUnpackForFeToBe(unpackfebe_cb);
      if (rc != LMON_OK) {
         err_printf("Failed LMON_be_regUnpackForBeToFe\n");
         return -1;
      } 
   }

   int rank, size;
   LMON_be_getMyRank(&rank);
   LMON_be_getSize(&size);
   debug_printf("Launchmon rank %d/%d\n", rank, size);

   rc = LMON_be_handshake(NULL);
   if (rc != LMON_OK) {
      err_printf("Failed LMON_be_handhshake\n");
      LMON_be_finalize();
      return -1;
   }

   rc = LMON_be_ready(NULL);
   if (rc != LMON_OK) {     
      err_printf("Failed LMON_be_ready\n");
      LMON_be_finalize();
      return -1;
   } 

   /* Receive port and unique_id via lmon. args is not fully 
      filled in at this point */
   {
      spindle_args_t args;
      rc = LMON_be_recvUsrData(&args);
      if (rc != LMON_OK) {
         err_printf("Failed to receive usr data from FE\n");
         return -1;
      }
      if (LMON_be_amIMaster() == LMON_YES) {
         conn_info.port = args.port;
         conn_info.num_ports = args.num_ports;
         conn_info.unique_id = args.unique_id;
      }
   }

   /* Broadcast port/secret from master to all servers */
   rc = LMON_be_broadcast(&conn_info, sizeof(conn_info));
   if (rc != LMON_OK) {
      err_printf("Failed to broadcast connection info\n");
      return -1;
   }
   
   result = spindleRunBE(conn_info.port, conn_info.num_ports, conn_info.unique_id, security_type,
                         releaseApplication);
   if (result == -1) {
      err_printf("Failed in call to spindleInitBE\n");
      return -1;
   }

   rc = LMON_be_finalize();
   if (rc != LMON_OK) {
      err_printf("Failed to finalize launchmon\n");
      return -1;
   }
   return 0;
}
