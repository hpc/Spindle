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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <alloca.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#if !defined(DEV_SHM)
#define DEV_SHM "/dev/shm/"
#endif

#if !defined(USE_GLIBC_SHM)

int shm_open_wrapper(const char *name, int oflag, mode_t mode)
{
   size_t name_size, dev_shm_size, new_name_size;
   char *new_name;
   int fd;

   while (*name == '/') name++;
   if (*name == '\0') {
      errno = EINVAL;
      return -1;
   }

   name_size = strlen(name);
   dev_shm_size = strlen(DEV_SHM);
   new_name_size = name_size + dev_shm_size + 1;
   if (name_size > 255) {
      errno = EINVAL;
      return -1;
   }
   
   new_name = alloca(new_name_size);
   memcpy(new_name, DEV_SHM, dev_shm_size);
   memcpy(new_name+dev_shm_size, name, name_size+1);
   
   fd = open(new_name, oflag | O_NOFOLLOW | O_CLOEXEC, mode);
   if (fd == -1)
      return -1;

   return fd;
}

int shm_unlink_wrapper(const char *name)
{
   size_t name_size, dev_shm_size, new_name_size;
   char *new_name;

   while (*name == '/') name++;
   if (*name == '\0') {
      errno = EINVAL;
      return -1;
   }

   name_size = strlen(name);
   dev_shm_size = strlen(DEV_SHM);
   new_name_size = name_size + dev_shm_size + 1;
   if (name_size > 255) {
      errno = EINVAL;
      return -1;
   }
   
   new_name = alloca(new_name_size);
   memcpy(new_name, DEV_SHM, dev_shm_size);
   memcpy(new_name+dev_shm_size, name, name_size+1);

   return unlink(new_name);
}

#else

int shm_open_wrapper(const char *name, int oflag, mode_t mode)
{
   return shm_open(name, oflag, mode);
}

int shm_unlink_wrapper(const char *name)
{
   return shm_unlink(name);
}

#endif
