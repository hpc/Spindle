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

#include "spindle.h"
#include "spindle_api_int.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>

/**
 * These are the functions that get called if Spindle isn't present.
 * If Spindle is present, then the functions in intercept_spindleapi.c
 * will be called instead of these.
 **/
int spindle_open(const char *pathname, int flags, ...)
{
   va_list argp;
   mode_t mode = (mode_t) 0;

   va_start(argp, flags);
   if (flags & O_CREAT)
      mode = va_arg(argp, mode_t);
   va_end(argp);
   return open(pathname, flags, mode);
}

int spindle_stat(const char *path, struct stat *buf)
{
   return stat(path, buf);
}

int spindle_lstat(const char *path, struct stat *buf)
{
   return lstat(path, buf);
}

FILE *spindle_fopen(const char *path, const char *mode)
{
   return fopen(path, mode);
}

void spindle_enable()
{
}

void spindle_disable()
{
}

int spindle_is_enabled()
{
   return 0;
}

int spindle_is_present()
{
   return 0;
}
