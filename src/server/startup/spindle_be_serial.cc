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

#include "spindle_launch.h"
#include "spindle_debug.h"
#include <cstdlib>

int startSerialBE(int /*argc*/, char * /*argv*/[], int security_type)
{
   char *port_s, *num_ports_s, *unique_id_s;
   unsigned port, num_ports;
   unique_id_t unique_id;

   port_s = getenv("SPINDLE_SERIAL_PORT");
   if (!port_s) {
      err_printf("SPINDLE_SERIAL_PORT wasn't set\n");
      return -1;
   }
   num_ports_s = getenv("SPINDLE_SERIAL_NUMPORTS");
   if (!num_ports_s) {
      err_printf("SPINDLE_SERIAL_NUMPORTS wasn't set\n");
      return -1;
   }
   unique_id_s = getenv("SPINDLE_SERIAL_SHARED");
   if (!unique_id_s) {
      err_printf("SPINDLE_SERIAL_SHARED wasn't set\n");
      return -1;
   }
   port = atoi(port_s);
   unique_id = strtoul(unique_id_s, NULL, 10);
   num_ports = atoi(num_ports_s);

   return spindleRunBE(port, num_ports, unique_id, security_type, NULL);
}
