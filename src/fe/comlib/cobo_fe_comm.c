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

#include "ldcs_cobo.h"
#include "cobo_comm.h"
#include "spindle_debug.h"
#include "ldcs_api.h"
#include "fe_comm.h"
#include "config.h"

int ldcs_audit_server_fe_md_open ( char **hostlist, int numhosts, unsigned int port, unsigned int shared_secret, 
                                   void **data  ) {
   int rc=0;
   int portlist[NUM_COBO_PORTS];
   int root_fd, ack;
   int i;

   for (i = 0; i < NUM_COBO_PORTS; i++) {
      portlist[i] = port + i;
   }

   debug_printf2("Opening with port %d - %d\n", portlist[0], portlist[NUM_COBO_PORTS-1]);
   cobo_server_open(shared_secret, hostlist, numhosts, portlist, NUM_COBO_PORTS);

   cobo_server_get_root_socket(&root_fd);
  
   ldcs_cobo_read_fd(root_fd, &ack, sizeof(ack));

   return(rc);
}

int ldcs_audit_server_fe_md_close ( void *data  ) {
  
   ldcs_message_t out_msg;
   int root_fd;

   debug_printf("Sending exit message to daemons\n");
   out_msg.header.type = LDCS_MSG_EXIT;
   out_msg.header.len = 0;
   out_msg.data = NULL;

   cobo_server_get_root_socket(&root_fd);
   write_msg(root_fd, &out_msg);
   return cobo_server_close();
}

int ldcs_audit_server_fe_broadcast(ldcs_message_t *msg, void *data)
{
   int root_fd;

   debug_printf("Broadcasting message to daemons\n");

   cobo_server_get_root_socket(&root_fd);
   return write_msg(root_fd, msg);
}

