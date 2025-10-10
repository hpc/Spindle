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

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include "ldcs_api.h"
#include "client.h"
#include "client_heap.h"
#include "client_api.h"
#include "should_intercept.h"
#include "patch_interception.h"

#define INTERCEPT_STAT
#if defined(INSTR_LIB)
#include "sym_alias.h"
#endif

extern void test_log(const char *name);

int (*orig_stat)(const char *path, struct stat *buf);
int (*orig_lstat)(const char *path, struct stat *buf);
int (*orig_xstat)(int vers, const char *path, struct stat *buf);
int (*orig_lxstat)(int vers, const char *path, struct stat *buf);
int (*orig_xstat64)(int vers, const char *path, struct stat *buf);
int (*orig_lxstat64)(int vers, const char *path, struct stat *buf);
int (*orig_fstat)(int fd, struct stat *buf);
int (*orig_fxstat)(int vers, int fd, struct stat *buf);
int (*orig_fxstat64)(int vers, int fd, struct stat *buf);

int handle_stat(const char *path, struct stat *buf, int flags)
{
   int result, exists;

   check_for_fork();
   if (ldcsid < 0 || !use_ldcs || !path || !buf) {
      debug_printf3("no ldcs: stat query %s\n", path ? path : "NULL");
      if (path)
         test_log(path);
      debug_printf3("Returning ORIG_STAT (%d)\n", (int) ORIG_STAT);
      return ORIG_STAT;
   }
   sync_cwd();

   debug_printf2("Spindle considering stat call %s%sstat%s(%s)\n", 
                 flags & IS_LSTAT ? "l" : "", 
                 flags & IS_XSTAT ? "x" : "",
                 flags & IS_64 ? "64" : "", 
                 path);

   if ((!(flags & FROM_LDSO)) && stat_filter(path) == ORIG_CALL) {
      /* Not used by stat, means run the original */
      debug_printf3("Allowing original stat on %s\n", path);
      return ORIG_STAT;
   }

   debug_printf3("Asking spindle for stat on %s\n", path);
   result = get_stat_result(ldcsid, path, flags & IS_LSTAT, &exists, buf);
   if (result == STAT_SELF_OPEN) {
      debug_printf3("Allowing original stat on %s\n", path);
      return ORIG_STAT;
   }
   else if (result == DISCONNECT) {
      debug_printf3("Disconnected. Using riginal stat on %s\n", path);
      return ORIG_STAT;
   }
   else if (result == -1) {
      /* Spindle level error */
      debug_printf3("Allowing original stat on %s\n", path);
      return ORIG_STAT;
   }

   if (!exists) {
      debug_printf3("File %s does not exist as per stat call\n", path);
      if (flags & FROM_LDSO)
         return ENOENT;
      else {
         errno = ENOENT;
         set_errno(ENOENT);
      }
      return -1;
   }
   
   debug_printf3("Ran file %s through spindle for stat\n", path);
   return 0;
}

static int get_pathname_from_fd(int fd, char* pathname, size_t len)
{
  char procpath[64];
  ssize_t rval;

  snprintf(procpath, sizeof(procpath), "/proc/self/fd/%d", fd);

  if ((rval = readlink(procpath, pathname, len)) < 0) {
    debug_printf3("readlink %s failed\n", procpath);
    set_errno(EBADF);
    errno = EBADF;
    return -1;
  }
  pathname[rval] = '\0';

  return 0;
}

static int handle_fstat(int fd, struct stat* buf, int flags)
{
   char path[MAX_PATH_LEN];

   if (fd_filter(fd) == ERR_CALL) {
      debug_printf("fstat hiding fd %d from application\n", fd);
      set_errno(EBADF);
      errno = EBADF;
      return -1;
   }

   if (get_pathname_from_fd(fd, path, sizeof(path)) < 0)
      return -1;

   debug_printf3("Redirecting fstat(%d=%s) to spindle\n", fd, path);
   return handle_stat(path, buf, flags);
}

int rtcache_stat(const char *path, struct stat *buf)
{
   int result = handle_stat(path, buf, 0);
   if (result != ORIG_STAT)
      return result;
   if (orig_stat) {
      result = orig_stat(path, buf);
   }
   else {
      result = stat(path, buf);
      if (result == -1) {
         set_errno(errno);
      }
   }   
   if (result == -1) {
      errno = get_errno();
      return -1;
   }
   return result;
}

int rtcache_lstat(const char *path, struct stat *buf)
{
   int result = handle_stat(path, buf, IS_LSTAT);
   if (result != ORIG_STAT)
      return result;
   result = orig_lstat(path, buf);
   if (result == -1) {
      errno = get_errno();
      return -1;
   }
   return result;   
}

int rtcache_xstat(int vers, const char *path, struct stat *buf)
{
   int result = handle_stat(path, buf, IS_XSTAT);
   if (result != ORIG_STAT) {
      return result;
   }
   result = orig_xstat(vers, path, buf);
   if (result == -1) {
      errno = get_errno();
      return -1;
   }
   return result;
}

int rtcache_xstat64(int vers, const char *path, struct stat *buf)
{
   int result = handle_stat(path, buf, IS_XSTAT | IS_64);
   if (result != ORIG_STAT)
      return result;
   result = orig_xstat64(vers, path, buf);
   if (result == -1) {
      errno = get_errno();
      return -1;
   }
   return result;
}

int rtcache_lxstat(int vers, const char *path, struct stat *buf)
{
   int result = handle_stat(path, buf, IS_LSTAT | IS_XSTAT);
   if (result != ORIG_STAT)
      return result;
   result = orig_lxstat(vers, path, buf);
   if (result == -1) {
      errno = get_errno();
      return -1;
   }
   return result;   
}

int rtcache_lxstat64(int vers, const char *path, struct stat *buf)
{
   int result = handle_stat(path, buf, IS_LSTAT | IS_XSTAT | IS_64);
   if (result != ORIG_STAT)
      return result;
   result = orig_lxstat64(vers, path, buf);
   if (result == -1) {
      errno = get_errno();
      return -1;
   }
   return result;   
}

int rtcache_fstat(int fd, struct stat *buf)
{
   int result = handle_fstat(fd, buf, 0);
   debug_printf3("handle_fstat returned %d (ORIG_STAT = %d)\n", result, (int) ORIG_STAT);
   if (result != ORIG_STAT)
      return result;
   debug_printf3("Calling orig_fstat on fd %d\n", fd);
   result = orig_fstat(fd, buf);
   if (result == -1) {
      errno = get_errno();
      return -1;
   }
   return result;   
}

int rtcache_fxstat(int vers, int fd, struct stat *buf)
{
   int result = handle_fstat(fd, buf, IS_XSTAT);
   debug_printf3("handle_fstat returned %d (ORIG_STAT = %d)\n", result, (int) ORIG_STAT);
   if (result != ORIG_STAT)
      return result;
   debug_printf3("Calling orig_fxstat on fd %d\n", fd);
   result = orig_fxstat(vers, fd, buf);
   if (result == -1) {
      errno = get_errno();
      return -1;
   }
   return result;   
}

int rtcache_fxstat64(int vers, int fd, struct stat *buf)
{
   int result;
   result = handle_fstat(fd, buf, IS_XSTAT | IS_64);
   debug_printf3("handle_fstat returned %d (ORIG_STAT = %d)\n", result, (int) ORIG_STAT);
   if (result != ORIG_STAT)
      return result;
  debug_printf3("Calling orig_fxstat64 on fd %d\n", fd);
   result = orig_fxstat64(vers, fd, buf);
   if (result == -1) {
      errno = get_errno();
      return -1;
   }
   return result;   
}

static int *ldso_errno;
int ldso_xstat(int ver, const char *filename, struct stat *buf)
{
   int result;
   result = handle_stat(filename, buf, FROM_LDSO);
   if (result != 0) {
      if (*ldso_errno)
         *ldso_errno = -result;
      return -1;
   }
   return 0;
}

int ldso_lxstat(int ver, const char *filename, struct stat *buf)
{
   int result;
   result = handle_stat(filename, buf, FROM_LDSO | IS_LSTAT);   
   if (result != 0) {
      if (*ldso_errno)
         *ldso_errno = -result;
      return -1;
   }
   return 0;
}

int init_intercept_ldso_stat()
{
   long int stat_offset = 0, lstat_offset = 0, errno_offset = 0;
   int result;
   
   result = get_ldso_metadata_statdata(&stat_offset, &lstat_offset, &errno_offset);
   if (result == -1) {
      debug_printf2("Not patching ld.so stat calls because ld.so metadata collection returned incomplete\n");
      return -1;
   }

   debug_printf2("Installing binary patch mapping ld.so's stat() mapping to our ldso_xstat()\n");
   result = install_ldso_patch(stat_offset, ldso_xstat);
   if (result == -1)
      return -1;

   debug_printf2("Installing binary patch mapping ld.so's lstat() mapping to our ldso_lxstat()\n");
   result = install_ldso_patch(lstat_offset, ldso_lxstat);
   if (result == -1)
      return -1;

   ldso_errno = calc_ldso_errno(errno_offset);
   return 0;
}
