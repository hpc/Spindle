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

#define _GNU_SOURCE

#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include "intercept.h"
#include "client.h"
#include "ldcs_api.h"
#include "client_api.h"

ssize_t (*orig_readlink)(const char *path, char *buf, size_t bufsiz);
int (*orig_readlinkat)(int dirfd, const char *pathname, char *buf, size_t bufsiz);

extern char *location;
extern int number;

static ssize_t readlink_worker(const char *path, char *buf, size_t bufsiz,
                               char *newbuf, ssize_t rl_result)
{
   char newpath[MAX_PATH_LEN+1];
   char spindle_id[32];
   int location_len;

   location_len = strlen(location);   
   snprintf(spindle_id, sizeof(spindle_id), "spindle.%d", number);

   if (!strstr(newbuf, spindle_id) ||
       strncmp(location, newbuf, location_len) != 0) {
      debug_printf3("readlink not intercepting, %s not prefixed by %s\n", newbuf, location);
      int len = strlen(newbuf);
      if (len > bufsiz)
         len = bufsiz;
      memcpy(buf, newbuf, len);
      return len;
   }

   send_orig_path_request(ldcsid, newbuf+location_len+1, newpath);
   int len = strlen(newpath);
   if (len > bufsiz)
      len = bufsiz;
   memcpy(buf, newpath, len);
   debug_printf2("readlink translated %s to %s to %s\n", path, newbuf, newpath);
   return len;
}

ssize_t readlink_wrapper(const char *path, char *buf, size_t bufsiz)
{
   char newbuf[MAX_PATH_LEN+1];
   ssize_t rl_result;
   debug_printf2("Intercepted readlink on %s\n", path);

   check_for_fork();

   memset(newbuf, 0, MAX_PATH_LEN+1);
   rl_result = orig_readlink(path, newbuf, MAX_PATH_LEN);
   if (rl_result == -1) {
      return -1;
   }

   return readlink_worker(path, buf, bufsiz, newbuf, rl_result);
}

int readlinkat_wrapper(int dirfd, const char *path, char *buf, size_t bufsiz)
{
   char newbuf[MAX_PATH_LEN+1];
   ssize_t rl_result;
   debug_printf2("Intercepted readlink on %s\n", path);

   check_for_fork();

   memset(newbuf, 0, MAX_PATH_LEN+1);
   rl_result = (ssize_t) orig_readlinkat(dirfd, path, newbuf, MAX_PATH_LEN);
   if (rl_result == -1) {
      return -1;
   }

   return (int) readlink_worker(path, buf, bufsiz, newbuf, rl_result);   
}
