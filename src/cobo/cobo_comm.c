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
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <netdb.h>
#include <errno.h>
#include <stdarg.h>
#include <poll.h>
#include <fcntl.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <poll.h>
#include "ldcs_cobo.h"
#include "spindle_debug.h"
#include "cobo_comm.h"

/* read size bytes into buf from fd, retry if necessary */
static int ldcs_cobo_read_fd_w_timeout(int fd, void* buf, int size, int usecs)
{
    int rc;
    int n = 0;
    char* offset = (char*) buf;

    struct pollfd fds;
    fds.fd      = fd;
    fds.events  = POLLIN;
    fds.revents = 0x0;

    while (n < size) {
        /* poll the connection with a timeout value */
        int poll_rc = poll(&fds, 1, usecs);
        if (poll_rc < 0) {
            err_printf("Polling file descriptor for read (read(fd=%d,offset=%p,size=%d) %m errno=%d)\n",
                       fd, offset, size-n, errno);
            return -1;
        } else if (poll_rc == 0) {
            return -1;
        }

        /* check the revents field for errors */
        if (fds.revents & POLLHUP) {
            debug_printf3("Hang up error on poll for read(fd=%d,offset=%p,size=%d)\n",
                          fd, offset, size-n);
            return -1;
        }

        if (fds.revents & POLLERR) {
            debug_printf3("Error on poll for read(fd=%d,offset=%p,size=%d)\n",
                          fd, offset, size-n);
            return -1;
        }

        if (fds.revents & POLLNVAL) {
            err_printf("Invalid request on poll for read(fd=%d,offset=%p,size=%d)\n",
                       fd, offset, size-n);
            return -1;
        }

        if (!(fds.revents & POLLIN)) {
            err_printf("No errors found, but POLLIN is not set for read(fd=%d,offset=%p,size=%d)\n",
                       fd, offset, size-n);
            return -1;
        }

        /* poll returned that fd is ready for reading */
	rc = read(fd, offset, size - n);

	if (rc < 0) {
	    if(errno == EINTR || errno == EAGAIN) { continue; }
            err_printf("Reading from file descriptor (read(fd=%d,offset=%p,size=%d) %m errno=%d)\n",
                       fd, offset, size-n, errno);
	    return rc;
	} else if(rc == 0) {
            err_printf("Unexpected return code of 0 from read from file descriptor (read(fd=%d,offset=%p,size=%d) revents=%x)\n",
                       fd, offset, size-n, fds.revents);
	    return -1;
	}

	offset += rc;
	n += rc;
    }

    return n;
}

/* read size bytes into buf from fd, retry if necessary */
int ldcs_cobo_read_fd(int fd, void* buf, int size)
{
    return ldcs_cobo_read_fd_w_timeout(fd, buf, size, -1);
}

/* write size bytes from buf into fd, retry if necessary */
static int ldcs_cobo_write_fd_w_suppress(int fd, void* buf, int size, int suppress)
{
    int rc;
    int n = 0;
    char* offset = (char*) buf;

    while (n < size) {
	rc = write(fd, offset, size - n);

	if (rc < 0) {
	    if(errno == EINTR || errno == EAGAIN) { continue; }
            if (!suppress) {
                err_printf("Writing to file descriptor (write(fd=%d,offset=%p,size=%d) %m errno=%d)\n",
                           fd, offset, size-n, errno);
            } else {
                debug_printf3("Writing to file descriptor (write(fd=%d,offset=%p,size=%d) %m errno=%d)\n",
                           fd, offset, size-n, errno);
            }
	    return rc;
	} else if(rc == 0) {
            if (!suppress) {
                err_printf("Unexpected return code of 0 from write to file descriptor (write(fd=%d,offset=%p,size=%d))\n",
                           fd, offset, size-n);
            } else {
                debug_printf3("Unexpected return code of 0 from write to file descriptor (write(fd=%d,offset=%p,size=%d))\n",
                           fd, offset, size-n);
            }
	    return -1;
	}

	offset += rc;
	n += rc;
    }

    return n;
}

/* write size bytes from buf into fd, retry if necessary */
int ldcs_cobo_write_fd(int fd, void* buf, int size)
{
    return ldcs_cobo_write_fd_w_suppress(fd, buf, size, 0);
}

int ll_read(int fd, void *buf, size_t count)
{
   int result;
   size_t pos = 0;

   while (pos < count) {
      result = read(fd, ((char *) buf) + pos, count - pos);
      debug_printf3("Read %d bytes from network: %d %d %d...\n", result, (int) ((char *)buf)[pos],
                    (int) ((char *)buf)[pos+1], (int) ((char *)buf)[pos+2]);
      if (result == -1 || result == 0) {
         if (errno == EINTR || errno == EAGAIN)
            continue;
         err_printf("Error reading from cobo FD %d\n", fd);
         return -1;
      }
      pos += result;
   }
   return 0;
}

int ll_write(int fd, void *buf, size_t count)
{
   int result;
   size_t pos = 0;

   while (pos < count) {
      result = write(fd, ((char *) buf) + pos, count - pos);
      debug_printf3("Wrote %d bytes to network: %d %d %d...\n", result, (int) ((char *)buf)[pos],
                    (int) ((char *)buf)[pos+1], (int) ((char *)buf)[pos+2]);

      if (result == -1 || result == 0) {
         if (errno == EINTR || errno == EAGAIN)
            continue;
         err_printf("Error writing to cobo FD %d\n", fd);
         return -1;
      }
      pos += result;
   }
   return 0;
}

int write_msg(int fd, ldcs_message_t *msg)
{
   int result = ll_write(fd, msg, sizeof(*msg));
   if (result == -1) {
      return -1;
   }

   if (msg->header.len && msg->data) {
      result = ll_write(fd, msg->data, msg->header.len);
      if (result == -1) {
         return -1;
      }
   }

   return 0;
}
