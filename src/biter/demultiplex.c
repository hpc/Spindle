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

#include "demultiplex.h"

#include <poll.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/select.h>

extern int is_client();

static int dequeue_message(int for_proc, void *msg_data, size_t msg_size, void *session)
{
   int result, queue_lock_held = 0, read_partial_message;
   size_t bytes_read = 0, bytes_to_read, qsize, qbytes_read;
   unsigned char *read_from, *write_to;
   void *qdata;

   if (!has_message(for_proc, session))
      return 0;
   
   result = take_queue_lock(session);
   if (result == -1) {
      goto error;
   }
   queue_lock_held = 1;

   while (has_message(for_proc, session) && bytes_read < msg_size) {
      get_message(for_proc, &qdata, &qsize, &qbytes_read, session);
      
      read_partial_message = (qsize - qbytes_read) > (msg_size - bytes_read);
      read_from = ((unsigned char *) qdata) + qbytes_read;
      write_to = ((unsigned char *) msg_data) + bytes_read;
      if (read_partial_message)
         bytes_to_read = msg_size - bytes_read;
      else
         bytes_to_read = qsize - qbytes_read;
      
      memcpy(write_to, read_from, bytes_to_read);
      bytes_read += bytes_to_read;
      qbytes_read += bytes_to_read;
      update_bytes_read(for_proc, qbytes_read, session);
      
      if (!read_partial_message) {
         assert(qbytes_read == qsize);
         rm_message(for_proc, session);
      }
   }

   result = release_queue_lock(session);
   if (result == -1) {
      goto error;
   }
   queue_lock_held = 0;

   return bytes_read;

  error:

   if (queue_lock_held)
      release_queue_lock(session);
   return -1;
}


static int check_for_msg(void *session, int id, void *buf, size_t size, size_t *bytes_read)
{
   unsigned char *recv_buf = ((unsigned char *) buf) + *bytes_read;
   size_t recv_buf_size = size - *bytes_read;
   int result;

   result = dequeue_message(id, recv_buf, recv_buf_size, session);
   if (result == -1) {
      set_last_error("Error in dequeue message");
      return -1;
   }
   else if (result == 0) {
      return 0;
   }
   else {
      *bytes_read += result;
      return result;
   }
}

static int reliable_read(int fd, void *buf, size_t size)
{
   int result;
   size_t bytes_read = 0;
   int delay_at = 100000;
   struct pollfd fds;

   while (bytes_read < size) {

      fds.fd = fd;
      fds.events = POLLIN;
      fds.revents = 0;
      result = poll(&fds, 1, 10000);
      if (result == -1) {
         return -1;
      }
      else if (result == 0) {
         usleep(10000); //.01 sec
         continue;
      }
      assert(result == 1);

      result = read(fd, ((unsigned char *) buf) + bytes_read, size - bytes_read);
      if (result == -1 && errno == EINTR)
         continue;
      if (result == -1 && errno == EAGAIN) {
         if (delay_at == 0)
            usleep(10000); //.01 sec
         else
            delay_at--;
         continue;
      }
      else if (result == -1) {
         return -1;
      }
      else if (result == 0) {
         return 0;
      }
      else
         bytes_read += result;
   }

   return bytes_read;
}

static int reliable_write(int fd, const void *buf, size_t size)
{
   static int init_sigpipe_handler = 0;
   int result;
   size_t bytes_written = 0;
   int delay_at = 100000;

   if (!init_sigpipe_handler) {
      struct sigaction act;
      memset(&act, 0, sizeof(act));
      act.sa_handler = SIG_IGN;
      result = sigaction(SIGPIPE, &act, NULL);
      if (result == -1) {
         return -1;
      }
      init_sigpipe_handler = 1;
   }

   while (bytes_written < size) {
      result = write(fd, ((unsigned char *) buf) + bytes_written, size - bytes_written);
      if (result == -1 && errno == EINTR)
         continue;
      if (result == -1 && errno == EAGAIN) {
         if (delay_at == 0)
            usleep(10000); //.01 sec
         else
            delay_at--;
         continue;
      }
      else if (result == 0 || (result == -1 && errno == EPIPE)) {
         return 0;
      }
      else if (result == -1)
         return -1;
      else
         bytes_written += result;
   }
   return bytes_written;
}

static int check_heap_blocked(void *session, int myid)
{
   msg_header_t msg;
   if (!is_heap_blocked(session))
      return 0;
   get_polled_data(session, &msg);
   return (myid != msg.msg_target);
}

int demultiplex_read(void *session, int fd, int myid, void *buf, size_t size)
{
   int result, have_pipe_lock = 0;
   size_t bytes_read = 0, recv_buf_size;
   msg_header_t msg_header;
   unsigned char *recv_buf;
   int read_partial_message;
   void *header_space;
   int iter_count = 100000;
   int error_return = -1;
   
   while (bytes_read < size) {
      if (iter_count == 0)
         usleep(10000); //.01 sec         
      else
         iter_count--;

      //Check for messages in queue
      result = check_for_msg(session, myid, buf, size, &bytes_read);
      if (result == -1) {
         goto error;
      }
      else if (bytes_read == size) {
         return bytes_read;
      }

      result = test_pipe_lock(session);
      if (result == -1) {
         goto error;
      }
      else if (result == 1) {
         have_pipe_lock = 1;

         result = check_for_msg(session, myid, buf, size, &bytes_read);
         if (result == -1) {
            goto error;
         }
         else if (bytes_read == size) {
            result = release_pipe_lock(session);
            if (result == -1) {
               goto error;
            }
            have_pipe_lock = 0;
            return bytes_read;
         }

         if (check_heap_blocked(session, myid)) {
            result = release_pipe_lock(session);
            if (result == -1) {
               goto error;
            }
            have_pipe_lock = 0;
            continue;
         }

         while (bytes_read < size) {
            //Have the pipe lock.  Get messages.
            if (!has_polled_data(session)) {
               result = reliable_read(fd, &msg_header, sizeof(msg_header));
               if (result == -1) {
                  set_last_error("Error reading header from server to client FD");
                  goto error;
               }
               if (result == 0) {
                  goto eof_error;
               }
            }
            else {
               get_polled_data(session, &msg_header);
               clear_polled_data(session);
            }

            if (msg_header.msg_target == myid) {
               //Message is for this process.
               read_partial_message = (msg_header.msg_size > size - bytes_read);
               recv_buf = ((unsigned char *) buf) + bytes_read;
               recv_buf_size = read_partial_message ? size - bytes_read : msg_header.msg_size;
               
               result = reliable_read(fd, recv_buf, recv_buf_size);
               if (result == -1) {
                  set_last_error("Error reading message from server");
                  goto error;
               }
               if (result == 0) {
                  goto eof_error;
               }
               bytes_read += result;
               msg_header.msg_size -= result;
            }

            if (msg_header.msg_size) {
               //Message is for someone else, or is the remainder left over after our read.
               result = get_message_space(msg_header.msg_size, &recv_buf, &header_space, session);
               if (result == -1) {
                  //OOM (likely client-side in shared heap).  Stash the msg_header, then hold-off
                  // any further reads until: 
                  //   A) the target process does the read
                  //     -or- 
                  //   B) we free up space in the heap
                  set_polled_data(session, msg_header);
                  set_heap_blocked(session);
                  break;
               }
               else {
                  set_heap_unblocked(session);
               }

               recv_buf_size = msg_header.msg_size;
               
               result = reliable_read(fd, recv_buf, recv_buf_size);
               if (result == -1) {
                  set_last_error("Error reading other process message from server");
                  goto error;
               }
               if (result == 0) {
                  goto eof_error;
               }

               result = enqueue_message(msg_header.msg_target, recv_buf, recv_buf_size, header_space, session);
               if (result == -1) {
                  goto error;
               }
            }
         }      

         result = release_pipe_lock(session);
         if (result == -1) {
            goto error;
         }
         have_pipe_lock = 0;
      }
   }

   return bytes_read;

  eof_error:
   error_return = 0;
  error:
   if (have_pipe_lock)
      release_pipe_lock(session);

   return error_return;
}

int demultiplex_write(void *session, int fd, int id, const void *buf, size_t size)
{
   int result;
   msg_header_t msg_header;
   int write_lock_held = 0;
   int error_return = -1;

   msg_header.msg_size = size;
   msg_header.msg_target = id;

   result = take_write_lock(session);
   if (result == -1)
      goto error;
   write_lock_held = 1;

   result = reliable_write(fd, &msg_header, sizeof(msg_header));
   if (result == -1) {
      set_last_error("Error writing header from client to server");
      goto error;
   }
   if (result == 0) 
      goto eof_error;

   result = reliable_write(fd, buf, size);
   if (result == -1) {
      set_last_error("Error writing message from client to server");
      goto error;
   }
   if (result == 0) 
      goto eof_error;

   result = release_write_lock(session);
   if (result == -1)
      goto error;
   write_lock_held = 0;

   return size;

  eof_error:
   error_return = 0;
  error:
   if (write_lock_held)
      release_write_lock(session);

   return error_return;
}

int demultiplex_poll(void *session, int fd, int *id)
{
   msg_header_t header;
   int has_pipe_lock = 0, result;
   if (has_polled_data(session)) {
      get_polled_data(session, &header);
      *id = header.msg_target;
      return 0;
   }

   result = take_pipe_lock(session);
   if (result == -1)
      goto error;
   has_pipe_lock = 1;

   result = reliable_read(fd, &header, sizeof(header));
   if (result == -1) {
      set_last_error("Error polling message from server");
      return -1;
   }
   
   set_polled_data(session, header);
   *id = header.msg_target;

   result = release_pipe_lock(session);
   if (result == -1)
      goto error;
   has_pipe_lock = 0;

   return 0;

  error:
   if (has_pipe_lock)
      release_pipe_lock(session);
   return -1;
}
