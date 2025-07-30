/*
This file is part of Spindle.  For copyright information see the COPYRIGHT 
file in the top level directory, or at 
https://github.com/hpc/Spindle/blob/master/COPYRIGHT

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License (as published by the Free Software
Foundation) version 2.1 dated February 1999.  This program is distributed in the
hope that it will be useful, but WITHOUT ANY WARRANTY; without even the IMPLIED
WARRANTY OF MECHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms 
and conditions of the GNU Lesser General Public License for more details.  You should 
have received a copy of the GNU Lesser General Public License along with this 
program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include "spindle_debug.h"
#include "fileutil.h"

int read_n_bytes(char *localname, int fd, void *buffer, size_t size)
{
   int result, error;
   size_t bytes_read;
   bytes_read = 0;
   debug_printf3("Reading %zu bytes from %s\n", size, localname);

   while (bytes_read != size) {
      errno = 0;
      result = read(fd, ((char *) buffer) + bytes_read, size - bytes_read);
      if (result == -1) {
         error = errno;
         if (error == EAGAIN || error == EINTR)
            continue;
         err_printf("Failed to read from file %s of size %zu (already read %zu): %s\n",
                    localname, size, bytes_read, strerror(error));
         close(fd);
         return -1;
      }
      bytes_read += result;
   }

   return 0;
}

int read_buffer(char *localname, char *buffer, int size)
{
   int result, bytes_read, fd;

   fd = open(localname, O_RDONLY);
   if (fd == -1) {
      err_printf("Failed to open %s for reading: %s\n", localname, strerror(errno));
      return -1;
   }

   bytes_read = 0;

   while (bytes_read != size) {
      result = read(fd, buffer + bytes_read, size - bytes_read);
      if (result <= 0) {
         if (errno == EAGAIN || errno == EINTR)
            continue;
         err_printf("Failed to read from file %s: %s\n", localname, strerror(errno));
         close(fd);
         return -1;
      }
      bytes_read += result;
   }
   close(fd);
   return 0;
}

int write_n_bytes(char *localname, int fd, void *buffer, size_t size)
{
   int result;
   size_t bytes_written = 0;

   while (bytes_written != size) {
      result = write(fd, ((char *) buffer) + bytes_written, size - bytes_written);
      if (result <= 0) {
         if (errno == EAGAIN || errno == EINTR)
            continue;
         err_printf("Failed to write to file %s: %s\n", localname, strerror(errno));
         close(fd);
         return -1;
      }
      bytes_written += result;
   }
   
   return 0;
}

int write_buffer(char *localname, char *buffer, size_t size)
{
   int result, fd;

   fd = creat(localname, 0600);
   if (fd == -1) {
      err_printf("Failed to create file %s for writing: %s\n", localname, strerror(errno));
      return -1;
   }

   result = write_n_bytes(localname, fd, buffer, size);
   if (result == -1)
      return -1;

   close(fd);
   return 0;
}
