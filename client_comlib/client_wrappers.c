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

#include "ldcs_api.h"
#include "config.h"

#if !defined(COMM)
#if defined(COMM_PIPES)
#define COMM pipe
#elif defined(COMM_SOCKETS)
#define COMM socket
#elif defined(COMM_SHMEM)
#define COMM shmem
#else
#error Unknown communication type
#endif
#endif

#define RENAME3(X, Y) X ## _ ## Y
#define RENAME2(X, Y) RENAME3(X, Y)
#define RENAME(X) RENAME2(X, COMM)

extern int RENAME(client_open_connection) (char* location, int number);
extern int RENAME(client_close_connection) (int connid);
extern int RENAME(client_register_connection) (char *connection_str);
extern char *RENAME(client_get_connection_string) (int fd);
extern int RENAME(client_send_msg) (int connid, ldcs_message_t * msg);
extern int RENAME(client_recv_msg_static) (int fd, ldcs_message_t *msg, ldcs_read_block_t block);

int client_open_connection(char* location, int number)
{
   return RENAME(client_open_connection) (location, number);
}

int client_close_connection(int connid)
{
   return RENAME(client_close_connection) (connid);
}

int client_register_connection(char *connection_str)
{
   return RENAME(client_register_connection) (connection_str);
}

char *client_get_connection_string(int fd)
{
   return RENAME(client_get_connection_string) (fd);
}

int client_send_msg(int connid, ldcs_message_t * msg)
{
   return RENAME(client_send_msg) (connid, msg);
}

int client_recv_msg_static(int fd, ldcs_message_t *msg, ldcs_read_block_t block)
{
   return RENAME(client_recv_msg_static) (fd, msg, block);
}
