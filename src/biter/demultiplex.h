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

#if !defined(_MSGREAD_H_)
#define _MSGREAD_H_

#include <stdint.h>
#include <unistd.h>

typedef struct msg_header_t {
   uint32_t msg_size;
   uint32_t msg_target;
} msg_header_t;

extern int test_pipe_lock(void *session);
extern int take_pipe_lock(void *session);
extern int release_pipe_lock(void *session);
extern void set_last_error(const char *errstr);
extern int take_queue_lock(void *session);
extern int release_queue_lock(void *session);
extern int take_write_lock(void *session);
extern int release_write_lock(void *session);

extern void set_polled_data(void *session, msg_header_t msg);
extern void get_polled_data(void *session, msg_header_t *msg);
extern void clear_polled_data(void *session);
extern int has_polled_data(void *session);

extern void set_heap_blocked(void *session);
extern void set_heap_unblocked(void *session);
extern int is_heap_blocked(void *session);

extern void get_message(int for_proc, void **msg_data, size_t *msg_size, size_t *bytes_read, void *session);
extern int has_message(int for_proc, void *session);
extern void rm_message(int for_proc, void *session);
extern int enqueue_message(int for_proc, void *msg_data, size_t msg_size, void *header_space, void *session);
extern int get_message_space(size_t msg_size, unsigned char **msg_space, void **header_space, void *session);
extern void update_bytes_read(int for_proc, size_t newval, void *session);

int demultiplex_read(void *session, int fd, int myid, void *buf, size_t size);
int demultiplex_write(void *session, int fd, int id, const void *buf, size_t size);
int demultiplex_poll(void *session, int fd, int *id);

#endif
