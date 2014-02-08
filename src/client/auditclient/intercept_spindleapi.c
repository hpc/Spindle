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
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>


#include "client.h"
#include "client_heap.h"
#include "spindle_debug.h"
#include "config.h"

static FILE* (*orig_spindle_fopen)(const char *pathname, const char *mode);

#if !defined(TLS)
#define TLS
#endif
static TLS int intercept_api_enabled = 0;
static TLS int under_spindle_call = 0;

static int int_spindle_open(const char *pathname, int flags, ...)
{
   va_list argp;
   mode_t mode = (mode_t) 0;
   int result;

   va_start(argp, flags);
   if (flags & O_CREAT)
      mode = va_arg(argp, mode_t);
   va_end(argp);

   debug_printf("User called spindle_open(%s, %d, %d)\n", pathname, flags, mode);

   under_spindle_call++;
   result = open_worker(pathname, flags, mode, 0);
   under_spindle_call--;
   return result;
}

static FILE *int_spindle_fopen(const char *path, const char *opts)
{
   FILE *result;
   under_spindle_call++;
   result = fopen_worker(path, opts, 0);
   under_spindle_call--;
   return result;
}

static int int_spindle_stat(const char *path, struct stat *buf)
{
   int result;
   debug_printf("User called spindle_stat(%s, %p)\n", path, buf);

   under_spindle_call++;
   result = handle_stat(path, buf, 0);
   under_spindle_call--;
   if (result != ORIG_STAT)
      return result;
   return stat(path, buf);
}

static int int_spindle_lstat(const char *path, struct stat *buf)
{
   debug_printf("User called spindle_lstat(%s, %p)\n", path, buf);

   under_spindle_call++;
   int result = handle_stat(path, buf, IS_LSTAT);
   under_spindle_call--;
   if (result != ORIG_STAT)
      return result;
   return lstat(path, buf);
}

static int int_spindle_is_present()
{
   return 1;
}

static void int_spindle_enable()
{
   intercept_api_enabled++;
}

static void int_spindle_disable()
{
   intercept_api_enabled--;
}

static int int_spindle_is_enabled()
{
   return (intercept_api_enabled > 0);
}

int relocate_spindleapi()
{
   return intercept_api_enabled > 0 || under_spindle_call;
}

ElfX_Addr redirect_spindleapi(const char *symname, ElfX_Addr value)
{
   if (strcmp(symname, "spindle_enable") == 0) {
      return (ElfX_Addr) int_spindle_enable;
   }
   else if (strcmp(symname, "spindle_disable") == 0) {
      return (ElfX_Addr) int_spindle_disable;
   }
   else if (strcmp(symname, "spindle_is_enabled") == 0) {
      return (ElfX_Addr) int_spindle_is_enabled;
   }
   else if (strcmp(symname, "spindle_is_present") == 0) {
      return (ElfX_Addr) int_spindle_is_present;
   }
   else if (strcmp(symname, "spindle_open") == 0) {
      return (ElfX_Addr) int_spindle_open;
   }
   else if (strcmp(symname, "spindle_stat") == 0) {
      return (ElfX_Addr) int_spindle_stat;
   }
   else if (strcmp(symname, "spindle_lstat") == 0) {
      return (ElfX_Addr) int_spindle_lstat;
   }
   else if (strcmp(symname, "spindle_fopen") == 0) {
      orig_spindle_fopen = (void *) value;
      return (ElfX_Addr) int_spindle_fopen;
   }
   else {
      return value;
   }
}

