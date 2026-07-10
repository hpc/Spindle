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

#include <errno.h>
#include <stddef.h>
#include <unistd.h>

#include "crash_io.h"

/* This file contains async-signal-safe functions for reading from
   and writing to file descriptors. Used by the crash handler to
   communicate with the server. */

int crash_raw_write(int fd, const void *buf, size_t len)
{
   const char *p = (const char *) buf;
   size_t left = len;
   while (left > 0) {
      int saved_errno = errno;
      ssize_t n = write(fd, p, left);
      int write_errno = errno;
      errno = saved_errno;
      if (n < 0) {
         if (write_errno == EINTR || write_errno == EAGAIN)
            continue;
         return -1;
      }
      if (n == 0)
         return -1;
      p += n;
      left -= (size_t) n;
   }
   return 0;
}

int crash_raw_read_exact(int fd, void *buf, size_t nbytes)
{
   char *p = (char *) buf;
   size_t left = nbytes;
   while (left > 0) {
      int saved_errno = errno;
      ssize_t n = read(fd, p, left);
      int read_errno = errno;
      errno = saved_errno;
      if (n < 0) {
         if (read_errno == EINTR || read_errno == EAGAIN)
            continue;
         return -1;
      }
      if (n == 0)
         return -1;
      p += n;
      left -= (size_t) n;
   }
   return 0;
}
