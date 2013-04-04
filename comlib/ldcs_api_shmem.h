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

#ifndef LDCS_API_SHMEM_H
#define LDCS_API_SHMEM_H

int ldcs_open_connection_shmem(char* location, int number);
int ldcs_close_connection_shmem(int fd);
int ldcs_create_server_shmem(char* location, int number);
int ldcs_open_server_connection_shmem(int fd);
int ldcs_open_server_connections_shmem(int fd, int *more_avail);
int ldcs_close_server_connection_shmem(int fd);
int ldcs_get_fd_shmem(int id);
int ldcs_destroy_server_shmem(int fd);
int ldcs_send_msg_shmem(int fd, ldcs_message_t * msg);
ldcs_message_t * ldcs_recv_msg_shmem(int fd, ldcs_read_block_t block );
int ldcs_recv_msg_static_shmem(int fd, ldcs_message_t *msg, ldcs_read_block_t block);
char *ldcs_get_connection_string_shmem(int fd);
int ldcs_register_connection_shmem(char *connection_str);

/* internal */
int _ldcs_write_shmem(int fd, const void *data, int bytes );
int _ldcs_read_shmem(int fd, void *data, int bytes, ldcs_read_block_t block );

/* additional API functions for SHMEM */
int ldcs_create_server_or_client_shmem(char* location, int number);
int ldcs_is_server_shmem(int ldcsid);
int ldcs_is_client_shmem(int ldcsid);

#endif

