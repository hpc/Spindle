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

#define SPINDLE_DO_EXPORT 1

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "handle_vararg.h"
#include "intercept.h"
#include "spindle_launch.h"

static void *spindle_malloc(size_t size)
{
   return malloc(size);
}

static void spindle_free(void *b)
{
   free(b);
}

static void *spindle_realloc(void *b, size_t size)
{
   return realloc(b, size);
}
 
SPINDLE_EXPORT int open(const char *pathname, int flags, ...);
SPINDLE_EXPORT int open64(const char *pathname, int flags, mode_t mode);
SPINDLE_EXPORT FILE *fopen(const char *pathname, const char *mode);
SPINDLE_EXPORT FILE *fopen64(const char *pathname, const char *mode);
SPINDLE_EXPORT int close(int fd);
SPINDLE_EXPORT int stat(const char *path, struct stat *buf);
SPINDLE_EXPORT int lstat(const char *path, struct stat *buf);
SPINDLE_EXPORT int __xstat(int vers, const char *path, struct stat *buf);
SPINDLE_EXPORT int __xstat64(int vers, const char *path, struct stat *buf);
SPINDLE_EXPORT int __lxstat(int vers, const char *path, struct stat *buf);
SPINDLE_EXPORT int __lxstat64(int vers, const char *path, struct stat *buf);
SPINDLE_EXPORT int fstat(int fd, struct stat *buf);
SPINDLE_EXPORT int fxstat(int vers, int fd, struct stat *buf);
SPINDLE_EXPORT int fxstat64(int vers, int fd, struct stat *buf);
SPINDLE_EXPORT int execl(const char *path, const char *arg0, ...);
SPINDLE_EXPORT int execv(const char *path, char *const argv[]);
SPINDLE_EXPORT int execle(const char *path, const char *arg0, ...);
SPINDLE_EXPORT int execve(const char *path, char *const argv[], char *const envp[]);
SPINDLE_EXPORT int execlp(const char *path, const char *arg0, ...);
SPINDLE_EXPORT int execvp(const char *path, char *const argv[]);
SPINDLE_EXPORT pid_t vfork();
SPINDLE_EXPORT int spindle_open(const char *pathname, int flags, ...);
SPINDLE_EXPORT int spindle_stat(const char *path, struct stat *buf);
SPINDLE_EXPORT int spindle_lstat(const char *path, struct stat *buf);
SPINDLE_EXPORT FILE *spindle_fopen(const char *path, const char *mode);
SPINDLE_EXPORT void spindle_enable();
SPINDLE_EXPORT void spindle_disable();
SPINDLE_EXPORT int spindle_is_enabled();
SPINDLE_EXPORT int spindle_is_present();
SPINDLE_EXPORT void spindle_test_log_msg(char *msg);

int open(const char *pathname, int flags, ...)
{
   int mode = 0;
   va_list arglist;

   if (flags & O_CREAT) {
      va_start(arglist, flags);
      mode = va_arg(arglist, int);
   }
   
   return rtcache_open(pathname, flags, mode);
}

int open64(const char *pathname, int flags, mode_t mode)
{
   return rtcache_open64(pathname, flags, mode);
}

FILE *fopen(const char *pathname, const char *mode)
{
   return rtcache_fopen(pathname, mode);
}

FILE *fopen64(const char *pathname, const char *mode)
{
   return rtcache_fopen64(pathname, mode);
}

int close(int fd)
{
   return rtcache_close(fd);
}

int stat(const char *path, struct stat *buf)
{
   return rtcache_stat(path, buf);
}

int lstat(const char *path, struct stat *buf)
{
   return rtcache_lstat(path, buf);
}

int __xstat(int vers, const char *path, struct stat *buf)
{
   return rtcache_xstat(vers, path, buf);
}

int __xstat64(int vers, const char *path, struct stat *buf)
{
   return rtcache_xstat64(vers, path, buf);
}

int __lxstat(int vers, const char *path, struct stat *buf)
{
   return rtcache_lxstat(vers, path, buf);   
}

int __lxstat64(int vers, const char *path, struct stat *buf)
{
   return rtcache_lxstat64(vers, path, buf);
}

int fstat(int fd, struct stat *buf)
{
   return rtcache_fstat(fd, buf);
}

int fxstat(int vers, int fd, struct stat *buf)
{
   return rtcache_fxstat(vers, fd, buf);
}

int fxstat64(int vers, int fd, struct stat *buf)
{
   return rtcache_fxstat64(vers, fd, buf);
}

int execl(const char *path, const char *arg0, ...)
{
   int result;
   VARARG_TO_ARGV;
   result = execv_wrapper(path, new_argv ? new_argv : argv);
   VARARG_TO_ARGV_CLEANUP;
   return result;
}

int execv(const char *path, char *const argv[])
{
   return execv_wrapper(path, argv);
}

int execle(const char *path, const char *arg0, ...)
{
   int result;
   char **envp;

   VARARG_TO_ARGV;
   envp = va_arg(arglist, char **);
   result = execve_wrapper(path, new_argv ? new_argv : argv, envp);
   VARARG_TO_ARGV_CLEANUP;

   return result;
}

int execve(const char *path, char *const argv[], char *const envp[])
{
   return execve_wrapper(path, argv, envp);
}

int execlp(const char *path, const char *arg0, ...)
{
   int result;
   VARARG_TO_ARGV;
   result = execvp_wrapper(path, new_argv ? new_argv : argv);
   VARARG_TO_ARGV_CLEANUP;
   return result;
}

int execvp(const char *path, char *const argv[])
{
   return execvp_wrapper(path, argv);
}

pid_t vfork()
{
   return vfork_wrapper();
}

int spindle_open(const char *pathname, int flags, ...)
{
   return int_spindle_open(pathname, flags);
}

int spindle_stat(const char *path, struct stat *buf)
{
   return int_spindle_stat(path, buf);
}

int spindle_lstat(const char *path, struct stat *buf)
{
   return int_spindle_lstat(path, buf);
}

FILE *spindle_fopen(const char *path, const char *mode)
{
   return int_spindle_fopen(path, mode);
}

void spindle_enable()
{
   return int_spindle_enable();
}

void spindle_disable()
{
   return int_spindle_disable();
}

int spindle_is_enabled()
{
   return int_spindle_is_enabled();
}

int spindle_is_present()
{
   return int_spindle_is_present();
}

void spindle_test_log_msg(char *msg)
{
   int_spindle_test_log_msg(msg);
}
