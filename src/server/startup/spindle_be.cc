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

#include "config.h"
#include "ldcs_audit_server_process.h"
#include "parseloc.h"
#include "ldcs_audit_server_md.h"
#include "ldcs_api.h"

#include <cstdlib>
#include <cassert>
#include <unistd.h>

extern int releaseApplication();

template<typename T>
void unpack_param(T &value, char *buffer, int &pos)
{
   memcpy(&value, buffer + pos, sizeof(T));
   pos += sizeof(T);
}

template<>
void unpack_param<char*>(char* &value, char *buffer, int &pos)
{
   unsigned int strsize = strlen(buffer + pos) + 1;
   value = (char *) malloc(strsize);
   strncpy(value, buffer + pos, strsize);
   pos += strsize;
}

static int unpack_data(spindle_args_t *args, void *buffer, int buffer_size)
{
   int pos = 0;
   char *buf = static_cast<char *>(buffer);
   unpack_param(args->number, buf, pos);
   unpack_param(args->port, buf, pos);
   unpack_param(args->num_ports, buf, pos);
   unpack_param(args->opts, buf, pos);
   unpack_param(args->shared_secret, buf, pos);
   unpack_param(args->use_launcher, buf, pos);
   unpack_param(args->startup_type, buf, pos);
   unpack_param(args->location, buf, pos);
   unpack_param(args->pythonprefix, buf, pos);
   unpack_param(args->preloadfile, buf, pos);
   assert(pos == buffer_size);

   return 0;    
}

int spindleRunBE(unsigned int port, unsigned int num_ports, unsigned int shared_secret, int (*post_setup)(spindle_args_t *))
{
   int result;
   spindle_args_t args;

   /* Setup network and share setup data */
   debug_printf3("spindleRunBE setting up network and receiving setup data\n");
   void *setup_data;
   int setup_data_size;
   result = ldcs_audit_server_network_setup(port, num_ports, shared_secret, &setup_data, &setup_data_size);
   if (result == -1) {
      err_printf("Error setting up network in spindleRunBE\n");
      return -1;
   }
   unpack_data(&args, setup_data, setup_data_size);
   free(setup_data);
   assert(args.shared_secret == shared_secret);
   assert(args.port == port);
   
   
   /* Expand environment variables in location. */
   char *new_location = parse_location(args.location);
   if (!new_location) {
      err_printf("Failed to convert location %s\n", args.location);
      return -1;
   }
   debug_printf("Translated location from %s to %s\n", args.location, new_location);
   free(args.location);
   args.location = new_location;

   result = ldcs_audit_server_process(&args);
   if (result == -1) {
      err_printf("Error in ldcs_audit_server_process\n");
      return -1;
   }

   if (post_setup) {
      result = post_setup(&args);
      if (result == -1) {
         err_printf("post_setup callback errored.  Returning\n");
         return -1;
      }
   }

   debug_printf("Setup done.  Running server.\n");
   ldcs_audit_server_run();
   if (result == -1) {
      err_printf("Error in ldcs_audit_server_process\n");
      return -1;
   }

   return 0;
}
