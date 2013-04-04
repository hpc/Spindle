/*
This file is part of Spindle.  For copyright information see the COPYRIGHT 
file in the top level directory, or at 
<TODO:URL>.

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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <sys/syscall.h>

#include "ldcs_api.h"
#include "ldcs_api_pipe.h"

int ldcs_open_connection(char* location, int number) {
  return(ldcs_open_connection_pipe(location,number));
}

char *ldcs_get_connection_string(int fd) {
   return ldcs_get_connection_string_pipe(fd);
}

int ldcs_register_connection(char *connection_str) {
   return ldcs_register_connection_pipe(connection_str);
}

int ldcs_close_connection(int fd) {
  return(ldcs_close_connection_pipe(fd));
}

int ldcs_create_server(char* location, int number) {
  return(ldcs_create_server_pipe(location,number));
}

int ldcs_open_server_connection(int fd) {
  return(ldcs_open_server_connection_pipe(fd));
}

int ldcs_open_server_connections(int fd, int *more_avail) {
  return(ldcs_open_server_connections_pipe(fd,more_avail));
}

int ldcs_close_server_connection(int fd) {
  return(ldcs_close_server_connection_pipe(fd));
}

int ldcs_get_fd(int id) {
  return(ldcs_get_fd_pipe(id));
}

int ldcs_destroy_server(int fd) {
  return(ldcs_destroy_server_pipe(fd));
}

int ldcs_send_msg(int fd, ldcs_message_t * msg) {
  return(ldcs_send_msg_pipe(fd,msg));
}

ldcs_message_t * ldcs_recv_msg(int fd, ldcs_read_block_t block ) {
  return(ldcs_recv_msg_pipe(fd,block));
}

int ldcs_recv_msg_static(int fd, ldcs_message_t *msg, ldcs_read_block_t block) {
  return(ldcs_recv_msg_static_pipe(fd, msg, block));
}
