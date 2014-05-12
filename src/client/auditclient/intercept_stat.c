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

static int (*orig_stat)(const char *path, struct stat *buf);
static int (*orig_lstat)(const char *path, struct stat *buf);
static int (*orig_xstat)(int vers, const char *path, struct stat *buf);
static int (*orig_lxstat)(int vers, const char *path, struct stat *buf);
static int (*orig_xstat64)(int vers, const char *path, struct stat *buf);
static int (*orig_lxstat64)(int vers, const char *path, struct stat *buf);
static int (*orig_fstat)(int fd, struct stat *buf);
static int (*orig_fxstat)(int vers, int fd, struct stat *buf);
static int (*orig_fxstat64)(int vers, int fd, struct stat *buf);

int handle_stat(const char *path, struct stat *buf, int flags)
{
   int result, exists;

   check_for_fork();
   if (ldcsid < 0 || !use_ldcs) {
      debug_printf3("no ldcs: stat query %s\n", path);
      return ORIG_STAT;
   }
   sync_cwd();

   debug_printf3("Spindle considering stat call %s%sstat%s(%s)\n", 
                 flags & IS_LSTAT ? "l" : "", 
                 flags & IS_XSTAT ? "x" : "",
                 flags & IS_64 ? "64" : "", 
                 path);

   if (stat_filter(path) == ORIG_CALL) {
      /* Not used by stat, means run the original */
      debug_printf3("Allowing original stat on %s\n", path);
      return ORIG_STAT;
   }

   result = get_stat_result(ldcsid, path, flags & IS_LSTAT, &exists, buf);
   if (result == -1) {
      /* Spindle level error */
      debug_printf3("Allowing original stat on %s\n", path);
      return ORIG_STAT;
   }

   if (!exists) {
      debug_printf3("File %s does not exist as per stat call\n", path);
      set_errno(ENOENT);
      return -1;
   }
   
   debug_printf3("Ran file %s through spindle for stat\n", path);
   return 0;
}

int handle_fstat(int fd)
{
   if (fd_filter(fd) == ERR_CALL) {
      debug_printf("fstat hiding fd %d from application\n", fd);
      set_errno(EBADF);
      return -1;
   }
   debug_printf2("Allowing original fstat(%d)\n", fd);
   return ORIG_STAT;
}

static int spindle_stat(const char *path, struct stat *buf)
{
   int result = handle_stat(path, buf, 0);
   if (result != ORIG_STAT)
      return result;
   return orig_stat(path, buf);
}

static int spindle_lstat(const char *path, struct stat *buf)
{
   int result = handle_stat(path, buf, IS_LSTAT);
   if (result != ORIG_STAT)
      return result;
   return orig_lstat(path, buf);
}

static int spindle_xstat(int vers, const char *path, struct stat *buf)
{
   int result = handle_stat(path, buf, IS_XSTAT);
   if (result != ORIG_STAT)
      return result;
   return orig_xstat(vers, path, buf);
}

static int spindle_xstat64(int vers, const char *path, struct stat *buf)
{
   int result = handle_stat(path, buf, IS_XSTAT | IS_64);
   if (result != ORIG_STAT)
      return result;
   return orig_xstat64(vers, path, buf);
}

static int spindle_lxstat(int vers, const char *path, struct stat *buf)
{
   int result = handle_stat(path, buf, IS_LSTAT | IS_XSTAT);
   if (result != ORIG_STAT)
      return result;
   return orig_lxstat(vers, path, buf);
}

static int spindle_lxstat64(int vers, const char *path, struct stat *buf)
{
   int result = handle_stat(path, buf, IS_LSTAT | IS_XSTAT | IS_64);
   if (result != ORIG_STAT)
      return result;
   return orig_lxstat64(vers, path, buf);
}

static int spindle_fstat(int fd, struct stat *buf)
{
   int result = handle_fstat(fd);
   if (result != ORIG_STAT)
      return result;
   return orig_fstat(fd, buf);
}

static int spindle_fxstat(int vers, int fd, struct stat *buf)
{
   int result = handle_fstat(fd);
   if (result != ORIG_STAT)
      return result;
   return orig_fxstat(vers, fd, buf);
}

static int spindle_fxstat64(int vers, int fd, struct stat *buf)
{
   int result = handle_fstat(fd);
   if (result != ORIG_STAT)
      return result;
   return orig_fxstat64(vers, fd, buf);
}

ElfX_Addr redirect_stat(const char *symname, ElfX_Addr value)
{
   if (strcmp(symname, "stat") == 0) {
      if (!orig_stat)
         orig_stat = (void *) value;
      return (ElfX_Addr) spindle_stat;
   }
   else if (strcmp(symname, "lstat") == 0) {
      if (!orig_lstat) 
         orig_lstat = (void *) value;
      return (ElfX_Addr) spindle_lstat;
   }
   /* glibc internal names */
   else if (strcmp(symname, "__xstat") == 0) {
      if (!orig_xstat)
         orig_xstat = (void *) value;
      return (ElfX_Addr) spindle_xstat;
   }
   else if (strcmp(symname, "__xstat64") == 0) {
      if (!orig_xstat64)
         orig_xstat64 = (void *) value;
      return (ElfX_Addr) spindle_xstat64;
   }
   else if (strcmp(symname, "__lxstat") == 0) {
      if (!orig_lxstat)
         orig_lxstat = (void *) value;
      return (ElfX_Addr) spindle_lxstat;
   }
   else if (strcmp(symname, "__lxstat64") == 0) {
      if (!orig_lxstat64)
         orig_lxstat64 = (void *) value;
      return (ElfX_Addr) spindle_lxstat64;
   }
   else if (strcmp(symname, "fstat") == 0) {
      if (!orig_fstat)
         orig_fstat = (void *) value;
      return (ElfX_Addr) spindle_fstat;
   }
   else if (strcmp(symname, "__fxstat") == 0) {
      if (!orig_fxstat)
         orig_fxstat = (void *) value;
      return (ElfX_Addr) spindle_fxstat;
   }
   else if (strcmp(symname, "__fxstat64") == 0) {
      if (!orig_fxstat64)
         orig_fxstat64 = (void *) value;
      return (ElfX_Addr) spindle_fxstat64;
   }
   else {
      debug_printf3("Skipped relocation of stat call %s\n", symname);
      return (ElfX_Addr) value;
   }
}
