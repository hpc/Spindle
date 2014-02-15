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

#include <stdlib.h>
#include "biterd.h"
#include "ldcs_api.h"
#include "config.h"
#include "spindle_debug.h"

int ldcs_create_server_biter(char* location, int number)
{
   return -1;
}

int ldcs_open_server_connection_biter(int serverid)
{
   return -1;
}

int ldcs_open_server_connections_biter(int fd, int *more_avail)
{
   return -1;
}

int ldcs_close_server_connection_biter(int connid)
{
   return -1;
}

int ldcs_destroy_server_biter(int cid)
{
   return -1;
}

int ldcs_send_msg_biter(int fd, ldcs_message_t *msg)
{
   return -1;
}

int ldcs_get_fd_biter(int fd) 
{
   return -1;
}

int ldcs_recv_msg_static_biter(int connid, ldcs_message_t *msg, ldcs_read_block_t block)
{
   return -1;
}
