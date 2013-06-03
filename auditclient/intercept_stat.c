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
#include "ldcs_api_opts.h"
#include "client.h"
#include "client_heap.h"
#include "client_api.h"

static int (*orig_stat)(const char *path, struct stat *buf);
static int (*orig_lstat)(const char *path, struct stat *buf);
static int (*orig_xstat)(int vers, const char *path, struct stat *buf);
static int (*orig_lxstat)(int vers, const char *path, struct stat *buf);
static int (*orig_xstat64)(int vers, const char *path, struct stat *buf);
static int (*orig_lxstat64)(int vers, const char *path, struct stat *buf);

static int should_do_existance_test(const char *path)
{
   /* Some hints to do existance tests.  We really want to just get python modules
      and libraries.  If the filename ends in a python or  */
   char *last_slash;
   char *last_dot;

   last_dot = strrchr(path, '.');
   if (last_dot &&
       (strcmp(last_dot, ".py") == 0 || 
        strcmp(last_dot, ".pyc") == 0 ||
        strcmp(last_dot, ".pyo") == 0 ||
        strcmp(last_dot, ".so") == 0)) {
      return 1;
   }

   last_slash = strrchr(path, '/');
   if (last_slash && strncmp(last_slash, "/lib", 4) == 0) {
      return 1;
   }

   return 0;
}

#define IS_64    (1 << 0)
#define IS_LSTAT (1 << 1)
#define IS_XSTAT (1 << 2)
static const char *handle_stat(const char *path, struct stat *buf, int flags)
{
   check_for_fork();

   if (should_do_existance_test(path)) {
      int exists, result;
      result = send_existance_test(ldcsid, (char *) path, &exists);
      if (result != -1 && !exists) {
         debug_printf3("Creating artifical ENOENT return for %s%sstat%s(%s)\n", 
                       flags & IS_LSTAT ? "l" : "", 
                       flags & IS_XSTAT ? "x" : "",
                       flags & IS_64 ? "64" : "", 
                       path);
         return NOT_FOUND_PREFIX;
      }
   }

   debug_printf3("Allowing call to %s%sstat%s(%s)\n", 
                 flags & IS_LSTAT ? "l" : "", 
                 flags & IS_XSTAT ? "x" : "",
                 flags & IS_64 ? "64" : "", 
                 path);
   return path;
}

static int spindle_stat(const char *path, struct stat *buf)
{
   return orig_stat(handle_stat(path, buf, 0), buf);
}

static int spindle_lstat(const char *path, struct stat *buf)
{
   return orig_lstat(handle_stat(path, buf, IS_LSTAT), buf);
}

static int spindle_xstat(int vers, const char *path, struct stat *buf)
{
   return orig_xstat(vers, handle_stat(path, buf, IS_XSTAT), buf);
}

static int spindle_xstat64(int vers, const char *path, struct stat *buf)
{
   return orig_xstat64(vers, handle_stat(path, buf, IS_XSTAT | IS_64), buf);
}

static int spindle_lxstat(int vers, const char *path, struct stat *buf)
{
   return orig_lxstat(vers, handle_stat(path, buf, IS_LSTAT | IS_XSTAT), buf);
}

static int spindle_lxstat64(int vers, const char *path, struct stat *buf)
{
   return orig_lxstat64(vers, handle_stat(path, buf, IS_LSTAT | IS_XSTAT | IS_64), buf);
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
   else {
      debug_printf3("Skipped relocation of stat call %s\n", symname);
      return (ElfX_Addr) value;
   }
}
