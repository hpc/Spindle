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

#include "handshake.h"
#include "ldcs_cobo.h"
#include "spindle_debug.h"
#include <syslog.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

int initialize_handshake_security(handshake_protocol_t *protocol)
{
   if (spindle_debug_prints > 2)
      handshake_enable_debug_prints(spindle_debug_output_f);

   assert(handshake_is_security_type_enabled(protocol->mechanism));
   cobo_set_handshake(protocol);
   handshake_enable_read_timeout(10);

   return 0;
}

void handle_security_error(const char *msg)
{
   char *prefix = "Spindle security error: ";
   char *newstr;
   size_t msg_size;

   msg_size = strlen(prefix) + strlen(msg) + 1;
   newstr = malloc(msg_size);
   assert(newstr);
   
   snprintf(newstr, msg_size, "%s%s", prefix, msg);

   openlog("spindle", LOG_CONS, LOG_USER);
   syslog(LOG_AUTHPRIV | LOG_ERR, "%s", newstr);
   closelog();
}
