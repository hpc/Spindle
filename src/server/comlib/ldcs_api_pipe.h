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

#ifndef LDCS_API_PIPE_H
#define LDCS_API_PIPE_H

#include "ldcs_api.h"

int ldcs_open_connection_pipe(char* location, int number);
int ldcs_close_connection_pipe(int fd);
int ldcs_create_server_pipe(char* location, int number);
int ldcs_open_server_connection_pipe(int fd);
int ldcs_open_server_connections_pipe(int fd, int *more_avail);
int ldcs_close_server_connection_pipe(int fd);
int ldcs_get_fd_pipe(int id);
int ldcs_destroy_server_pipe(int fd);
int ldcs_send_msg_pipe(int fd, ldcs_message_t * msg);
ldcs_message_t * ldcs_recv_msg_pipe(int fd, ldcs_read_block_t block );
int ldcs_recv_msg_static_pipe(int fd, ldcs_message_t *msg, ldcs_read_block_t block);
char *ldcs_get_connection_string_pipe(int fd);
int ldcs_register_connection_pipe(char *connection_str);

/* internal */
int _ldcs_write_pipe(int fd, const void *data, int bytes );
int _ldcs_read_pipe(int fd, void *data, int bytes, ldcs_read_block_t block );

typedef enum {
   LDCS_PIPE_FD_TYPE_SERVER,
   LDCS_PIPE_FD_TYPE_CONN,
   LDCS_CLIENT_STATUS_UNKNOWN
} fd_list_entry_type_t;

struct fdlist_entry_t
{
  int   inuse;
  fd_list_entry_type_t type;

  /* server part */
  int   notify_fd; 
  int   conn_list_size; 
  int   conn_list_used; 
  int  *conn_list; 
  char *path; 

  /* connection part */
  int   in_fd;
  char *in_fn;
  int   out_fd;
  char *out_fn;
  int   serverfd;
};

#endif

