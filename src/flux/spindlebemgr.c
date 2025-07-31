/*
This file is part of Spindle.  For copyright information see the COPYRIGHT 
file in the top level directory, or at 
https://github.com/hpc/Spindle/blob/master/COPYRIGHT

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License (as published by the Free Software
Foundation) version 2.1 dated February 1999.  This program is distributed in the
hope that it will be useful, but WITHOUT ANY WARRANTY; without even the IMPLIED
WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms 
and conditions of the GNU Lesser General Public License for more details.  You should 
have received a copy of the GNU Lesser General Public License along with this 
program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include "spindlemgr.h"
#include "fluxmgr.h"
#include "procmgr.h"
#include "spindle_launch.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#define BE_TIMEOUT_SECONDS 30

static int decode_daemon_args(char *daemon_arg_str, unsigned int *port, unsigned int *num_ports, unique_id_t *unique_id, int *security_type);

static int post_setup(spindle_args_t *params)
{
   params=params;
   ping_readymsg(0);
   clean_readymsg();
   return 0;
}

int run_be(int argc, char **argv)
{
   argc=argc;
   argv=argv;
   int result;
   char *daemon_arg_str;
   unsigned int port, num_ports;
   unique_id_t unique_id;
   int security_type;

   result = fluxmgr_init();
   if (result == -1) {
      spindle_debug_printf(1, "ERROR: Could not init flux mgr\n");
      ping_readymsg(1);      
      return -1;
   }
   spindle_debug_printf(1, "Waiting for BE info during session startup\n");
   result = fluxmgr_get_from_kvs(&daemon_arg_str, BE_TIMEOUT_SECONDS);
   if (result < 0) {
      spindle_debug_printf(1, "ERROR: Could not get daemon args from kvs\n");
      ping_readymsg(1);
      fluxmgr_close();
      return -1;
   }
   spindle_debug_printf(2, "Got daemon_args '%s'\n", daemon_arg_str);
   result = decode_daemon_args(daemon_arg_str, &port, &num_ports, &unique_id, &security_type);
   if (result == -1) {
      spindle_debug_printf(1, "ERROR: Could not decode daemon args str: '%s'\n", daemon_arg_str);
      free(daemon_arg_str);
      ping_readymsg(1);
      fluxmgr_close();      
      return -1;
   }
   free(daemon_arg_str);
   fluxmgr_close();

   spindle_debug_printf(1, "Running BE session\n");
   result = spindleRunBE(port, num_ports, unique_id, security_type, post_setup);
   if (result == -1) {
      spindle_debug_printf(1, "spindleRunBE returned an error\n");
      return -1;
   }
   return 0;
}

static int decode_daemon_args(char *daemon_arg_str, unsigned int *port, unsigned int *num_ports, unique_id_t *unique_id, int *security_type)
{
   int result;
   result = sscanf(daemon_arg_str, "%u %u %lu %d", port, num_ports, unique_id, security_type);
   if (result != 4)
      return -1;
   return 0;
}
