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
#include "ldcs_api.h"

#if !defined(COMM)
#if defined(COMM_PIPES)
#define COMM pipe
#elif defined(COMM_SOCKETS)
#define COMM socket
#elif defined(COMM_BITER)
#define COMM biter
#elif defined(COMM_SHMEM)
#define COMM shmem
#else
#error Unknown communication type
#endif
#endif

#define RENAME3(X, Y) X ## _ ## Y
#define RENAME2(X, Y) RENAME3(X, Y)
#define RENAME(X) RENAME2(X, COMM)

extern int RENAME(ldcs_create_server) (char* location, int number);
extern int RENAME(ldcs_open_server_connection) (int serverid);
extern int RENAME(ldcs_open_server_connections) (int fd, int *more_avail);
extern int RENAME(ldcs_close_server_connection) (int connid);
extern int RENAME(ldcs_destroy_server) (int cid);
extern int RENAME(ldcs_send_msg) (int fd, ldcs_message_t *msg);
extern int RENAME(ldcs_get_fd)(int fd);
extern int RENAME(ldcs_recv_msg_static)(int connid, ldcs_message_t *msg, ldcs_read_block_t block);

int ldcs_create_server(char* location, int number)
{
   return RENAME(ldcs_create_server)(location, number);
}

int ldcs_open_server_connection(int serverid)
{
   return RENAME(ldcs_open_server_connection)(serverid);
}

int ldcs_open_server_connections(int fd, int *more_avail)
{
   return RENAME(ldcs_open_server_connections)(fd, more_avail);
}

int ldcs_close_server_connection(int connid)
{
   return RENAME(ldcs_close_server_connection)(connid);
}

int ldcs_destroy_server(int cid)
{
   return RENAME(ldcs_destroy_server)(cid);
}

int ldcs_send_msg(int fd, ldcs_message_t *msg)
{
   return RENAME(ldcs_send_msg)(fd, msg);
}

int ldcs_get_fd(int fd) 
{
   return RENAME(ldcs_get_fd)(fd);
}

int ldcs_recv_msg_static(int connid, ldcs_message_t *msg, ldcs_read_block_t block)
{
   return RENAME(ldcs_recv_msg_static)(connid, msg, block);
}
