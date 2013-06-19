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

#include "spindle_launch.h"

static int unpackfebe_cb(void* udatabuf, 
                         int udatabuflen, 
                         void* udata) 
{
   spindle_args_t *args = (spindle_args_t *) udata;
   
   char *buffer = (char *) udatabuf;
   int pos = 0;

   memcpy(&args->port, buffer + pos, sizeof(args->port));
   pos += sizeof(args->port);
   
   memcpy(&args->shared_secret, buffer + pos, sizeof(args->shared_secret));
   pos += sizeof(args->shared_secret);

   assert(pos == udatabuflen);

   return 0;    
}

int startLaunchmonBE(int argc, char *argv[])
{
   struct {
      unsigned int port;
      unsigned int shared_secret;
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

   /* Recieve port and shared_secret via lmon. args is not fully 
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
         conn_info.shared_secret = args.shared_secret;
      }
   }

   /* Broadcast port/secret from master to all servers */
   rc = LMON_be_broadcast(conn_info, sizeof(conn_info));
   if (rc != LMON_OK) {
      err_printf("Failed to broadcast connection info\n");
      return -1;
   }
   
   result = spindleRunBE(conn_info.port, conn_info.shared_secret);
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
