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
#include <errno.h>
#include "intercept.h"
#include "client.h"
#include "ldcs_api.h"
#include "client_api.h"
#include "spindle_launch.h"
#include "should_intercept.h"

ssize_t (*orig_readlink)(const char *path, char *buf, size_t bufsiz);
ssize_t (*orig_readlinkat)(int dirfd, const char *pathname, char *buf, size_t bufsiz);

extern char *location;

static int fix_local_readlink(char *buf, size_t bufsiz)
{
   char spindle_id[32];
   int location_len, result;
   char tmp[MAX_PATH_LEN+1];

   location_len = strlen(location);   
   snprintf(spindle_id, sizeof(spindle_id), "spindle.%lx", number);
   if (strstr(buf, spindle_id) && strncmp(location, buf, location_len) == 0) {
      debug_printf2("readlink received spindle cache path %s. Translating\n", buf);
      result = send_orig_path_request(ldcsid, buf+location_len+1, tmp);
      if (result == -1)
         return -1;
      debug_printf2("readlink translated spindle local path %s to %s\n", buf, tmp);
      strncpy(buf, tmp, bufsiz);
      return 0;
   }
   return 0;
}

ssize_t readlink_wrapper(const char *path, char *buf, size_t bufsiz)
{
   char resultpath[MAX_PATH_LEN+1];
   int intercept_result, result, readlink_errcode;
   size_t len;
   ssize_t rl_result;

   check_for_fork();

   memset(resultpath, 0, sizeof(resultpath));
   intercept_result = stat_filter(path);
   if (intercept_result == ORIG_CALL) {
      debug_printf3("Not translating readlink(%s)\n", path);
      result = orig_readlink(path, resultpath, sizeof(resultpath));
      if (result == -1) {
         errno = get_errno();         
         return -1;
      }
      resultpath[sizeof(resultpath)-1] = '\0';
   }
   else if (intercept_result == REDIRECT) {
      debug_printf3("Intercepting readlink(%s)\n", path);
      result = get_readlink_result(ldcsid, path, resultpath, &rl_result, &readlink_errcode);
      if (result == -1 || result == STAT_SELF_OPEN) {
         if (result == -1)
            err_printf("Spindle readlink returned error. Using orig readlink\n");
         else
            debug_printf3("Spindle readlink returned self open. Using orig readlink\n");
         result = orig_readlink(path, resultpath, sizeof(resultpath));
         if (result == -1) {
            errno = get_errno();            
            return -1;
         }
         resultpath[sizeof(resultpath)-1] = '\0';
      }
      else {
         resultpath[sizeof(resultpath)-1] = '\0';
         if (rl_result == 0) {
            debug_printf2("readlink(%s) returned error EINVAL\n", path);
            set_errno(EINVAL);
            errno = EINVAL;
            return -1;
         }
         else if (readlink_errcode) {
            debug_printf2("readlink(%s) returned error %d\n", path, readlink_errcode);
            set_errno((int) readlink_errcode);
            errno = (int) readlink_errcode;
            return -1;
         }
         else {
            debug_printf3("readlink(%s) returned result %ld with %.*s\n", path, rl_result,
                          (int) rl_result, resultpath);
         }
      }
   }

   result = fix_local_readlink(resultpath, sizeof(resultpath));
   if (result == -1) {
      err_printf("Could not fix local path %s\n", resultpath);
   }

   len = strlen(resultpath);
   if (len > bufsiz)
      len = bufsiz;
   memcpy(buf, resultpath, len);
   debug_printf2("spindle readlink translated %s to %.*s with len %zu\n", path, (int)len, buf, len);
   return len;
}

ssize_t readlinkat_wrapper(int dirfd, const char *path, char *buf, size_t bufsiz)
{
   char newbuf[MAX_PATH_LEN+1];
   ssize_t rl_result;
   size_t len;
   int result;
   
   debug_printf2("Intercepted readlinkat on %s\n", path);

   check_for_fork();

   memset(newbuf, 0, MAX_PATH_LEN+1);
   rl_result = (ssize_t) orig_readlinkat(dirfd, path, newbuf, MAX_PATH_LEN);
   if (rl_result == -1) {
      errno = get_errno();
      return -1;
   }

   result = fix_local_readlink(newbuf, sizeof(newbuf));
   if (result == -1) {
      err_printf("Could not fix local path %s\n", newbuf);
   }

   len = strlen(newbuf);
   if (len > bufsiz)
      len = bufsiz;
   memcpy(buf, newbuf, len);
   debug_printf2("spindle readlink translated %s to %.*s with len %zu\n", path, (int)len, newbuf, len);
   return (ssize_t) len;
}
