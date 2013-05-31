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

#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>

#include "ldcs_api.h"
#include "ldcs_api_opts.h"
#include "client.h"
#include "client_heap.h"
#include "client_api.h"

static int (*orig_open)(const char *pathname, int flags, ...);
static int (*orig_open64)(const char *pathname, int flags, ...);
static FILE* (*orig_fopen)(const char *pathname, const char *mode);
static FILE* (*orig_fopen64)(const char *pathname, const char *mode);

/**
 * open_filter returns 1 if we should redirect an open call
 * to a cached location, or 0 if we shouldn't.
 *
 * Currently setup to redirect reads of .py and .pyc files.
 **/
static int open_filter(const char *fname)
{
   const char *last_dot;
   if (opts & OPT_RELOCPY) {
      last_dot = strrchr(fname, '.');
      if (!last_dot)
         return 0;
      if (strcmp(last_dot, ".py") == 0 || strcmp(last_dot, ".pyc") == 0 || strcmp(last_dot, ".so") == 0)
         return 1;
   }

   return 0;
}

static int open_filter_flags(int flags)
{
   return !(((flags & O_WRONLY) == O_WRONLY) || ((flags & O_RDWR) == O_RDWR));
}

static int open_filter_str(const char *mode)
{
   if (!mode)
      return 1;
   while (*mode) {
      if (*mode == 'w' || *mode == 'a')
         return 0;
      mode++;
   }
   return 1;
}


/* returns:
   0 if not existent
   -1 could not check, use orig open
   1 exists, newpath contains real location */
static int do_check_file(const char *path, char **newpath) {
   char *myname, *newname;
  
   myname=(char *) path;

   check_for_fork();
   if (ldcsid < 0 || !use_ldcs) {
      debug_printf3("no ldcs: open file query %s\n", myname);
      return -1;
   }
   sync_cwd();

   debug_printf2("Open operation requesting file: %s\n", path);
   send_file_query(ldcsid, myname, &newname);
   debug_printf("Open file request returned %s -> %s\n", path, newname ? newname : "NULL");

   if (newname != NULL) {
      *newpath=newname;
      debug_printf3("file found under path %s\n", *newpath);
      return 1;
   } else {
      *newpath=NULL;
      errno = ENOENT;
      debug_printf3("file not found file, set errno to ENOENT\n");
      return(0);
   }
}

static int rtcache_open(const char *path, int oflag, ...)
{
   va_list argp;
   mode_t mode;
   int rc;
   char *newpath;
   
   va_start(argp, oflag);
   mode = va_arg(argp, mode_t);
   va_end(argp);

   debug_printf3("open redirection of %s\n", path);

   if (!open_filter(path) || !open_filter_flags(oflag) || (ldcsid<0))
      return orig_open(path, oflag, mode);

   rc = do_check_file(path, &newpath);
   if (rc == 0) {
      set_errno(ENOENT);
      return -1;
   }
   else if (rc < 0) {
      return orig_open(path, oflag, mode);
   } 
   else {
      debug_printf("Redirecting 'open' call, %s to %s\n", path, newpath);
      rc = orig_open(newpath, oflag, mode);
      spindle_free(newpath);
      return rc;
   }
}

static int rtcache_open64(const char *path, int oflag, ...)
{
   va_list argp;
   mode_t mode;
   int rc;
   char *newpath;

   va_start(argp, oflag);
   mode = va_arg(argp, mode_t);
   va_end(argp);

   debug_printf3("open64 redirection of %s\n", path);

   if (!open_filter(path) || !open_filter_flags(oflag) || (ldcsid<0))
      return orig_open64(path, oflag, mode);

   rc = do_check_file(path, &newpath);

   if (rc==0) {
      set_errno(ENOENT);
      return -1;
   }
   else if (rc < 0) {
      return orig_open64(path, oflag, mode);
   } 
   else {
      debug_printf("Redirecting 'open64' call, %s to %s\n", path, newpath);
      rc = orig_open64(newpath, oflag, mode);
      spindle_free(newpath);
      return rc;
   }
}

static FILE *rtcache_fopen(const char *path, const char *mode)
{
   int rc;
   char *newpath;
   FILE *result;

   debug_printf3("fopen redirection of %s\n", path);
   if (!open_filter(path) || !open_filter_str(mode) || (ldcsid<0))
      return orig_fopen(path, mode);

   rc = do_check_file(path, &newpath);
   if(rc==0) {
      set_errno(ENOENT);
      return NULL;
   }
   else if (rc<0) {
      return orig_fopen(path, mode);
   } 
   else {
      debug_printf("Redirecting 'fopen' call, %s to %s\n", path, newpath);
      result = orig_fopen(newpath, mode);
      spindle_free(newpath);
      return result;
   }
}

static FILE *rtcache_fopen64(const char *path, const char *mode)
{
   int rc;
   char *newpath=NULL;
   FILE *fp;

   debug_printf3("fopen64 redirection of %s\n", path);

   if (!open_filter(path) || !open_filter_str(mode) || (ldcsid<0))
      return orig_fopen64(path, mode);
   rc = do_check_file(path, &newpath);

   if(rc == 0) {
      set_errno(ENOENT);
      return NULL;
   }
   else if (rc < 0) {
      return orig_fopen64(path, mode);
   } 
   else {
      debug_printf("Redirecting 'fopen64' call, %s to %s\n", path, newpath);
      fp = orig_fopen64(newpath, mode);
      spindle_free(newpath);
      return fp; 
   }
}

ElfX_Addr redirect_open(const char *symname, ElfX_Addr value)
{
   if (strcmp(symname, "open") == 0) {
      if (!orig_open)
         orig_open = (void *) value;
      return (ElfX_Addr) rtcache_open;
   }
   else if (strcmp(symname, "open64") == 0) {
      if (!orig_open64)
         orig_open64 = (void *) value;
      return (ElfX_Addr) rtcache_open64;
   }
   else if (strcmp(symname, "fopen") == 0) {
      if (!orig_fopen)
         orig_fopen = (void *) value;
      return (ElfX_Addr) rtcache_fopen;
   }
   else if (strcmp(symname, "fopen64") == 0) {
      if (!orig_fopen64)
         orig_fopen64 = (void *) value;
      return (ElfX_Addr) rtcache_fopen64;
   }
   else
      return (ElfX_Addr) value;
}
