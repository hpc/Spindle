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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#include "spindle_launch.h"
#include "client.h"
#include "client_api.h"
#include "should_intercept.h"
#include "spindle_debug.h"

extern int relocate_spindleapi();

static int is_python_path(const char *pathname)
{
   unsigned int i;

   assert(pythonprefixes);
   for (i = 0; pythonprefixes[i].path != NULL; i++) {
      if (strncmp(pythonprefixes[i].path, pathname, pythonprefixes[i].pathsize) == 0)
         return 1;
   }

   return 0;
}

static int is_python(const char *pathname, char *last_dot)
{
   if (last_dot &&
       (strcmp(last_dot, ".py") == 0 ||
        strcmp(last_dot, ".pyc") == 0 ||
        strcmp(last_dot, ".pyo") == 0))
      return 1;
   return 0;
}

static int is_compiled_python(const char *pathname, char *last_dot)
{
   if (last_dot &&
       (strcmp(last_dot, ".pyc") == 0 ||
        strcmp(last_dot, ".pyo") == 0))
      return 1;
   return 0;
}

static int is_julia(const char *pathname, char *last_dot)
{
   if (last_dot &&
       (strcmp(last_dot, ".jl") == 0 ||
        strcmp(last_dot, ".ji") == 0))
      return 1;
   return 0;
}

static int is_dso(const char *pathname, char *last_slash, char *last_dot)
{
   if (last_dot &&
       strcmp(last_dot, ".so") == 0)
      return 1;

   if (last_slash && 
       strstr(last_slash, ".so."))
      return 1;

   return 0;
}

static int is_lib_prefix(const char *pathname, char *last_slash)
{
   if (last_slash && strncmp(last_slash, "/lib", 4) != 0)
      return 0;
   if (!last_slash && strncmp(pathname, "lib", 3) != 0)
      return 0;
   int len = strlen(pathname);
   if (!strstr(last_slash, ".so.") && len > 3 && strncmp(pathname + len - 3, ".so", 3) != 0)
      return 0;
   return 1;
}

#define open_for_write(X) ((X & O_WRONLY) == O_WRONLY || (X & O_RDWR) == O_RDWR)
#define open_for_excl(X) ((X & (O_WRONLY|O_CREAT|O_EXCL|O_TRUNC)) == (O_WRONLY|O_CREAT|O_EXCL|O_TRUNC))
#define open_for_dir(X) (X & O_DIRECTORY)

int open_filter(const char *fname, int flags)
{
   char *last_slash, *last_dot;

   if (relocate_spindleapi()) {
      if (open_for_excl(flags))
         return EXCL_OPEN;
      if (!open_for_write(flags))
         return REDIRECT;
      return ORIG_CALL;
   }

   last_dot = strrchr(fname, '.');
   last_slash = strrchr(fname, '/');

   if (opts & OPT_RELOCJL) {
      if (is_julia(fname, last_dot) && !open_for_dir(flags))
         return REDIRECT;
   }

   if (!(opts & OPT_RELOCPY))
      return ORIG_CALL;

   if (is_python_path(fname) && !open_for_dir(flags))
      return REDIRECT;

   if (!open_for_write(flags) && is_dso(fname, last_slash, last_dot))
      return REDIRECT;

   if (open_for_excl(flags) && is_compiled_python(fname, last_dot))
      return EXCL_OPEN;

   if (!open_for_write(flags) && is_python(fname, last_dot))
      return REDIRECT;

   return ORIG_CALL;
}

#undef open_for_write
#undef open_for_excl
#define open_for_write(X) (*X == 'w' || *X == 'a')
#define open_for_excl(X) (*X == 'x')
int fopen_filter(const char *fname, const char *flags)
{
   char *last_slash, *last_dot;

   if (relocate_spindleapi()) {
      if (open_for_excl(flags))
         return EXCL_OPEN;
      if (!open_for_write(flags))
         return REDIRECT;
      return ORIG_CALL;
   }

   last_dot = strrchr(fname, '.');
   last_slash = strrchr(fname, '/');

   if (opts & OPT_RELOCJL) {
      if (!open_for_write(flags) && is_julia(fname, last_dot))
         return REDIRECT;
   }

   if (!(opts & OPT_RELOCPY))
      return ORIG_CALL;

   if (is_python_path(fname))
      return REDIRECT;

   if (!open_for_write(flags) && is_dso(fname, last_slash, last_dot))
      return REDIRECT;

   if (open_for_excl(flags) && is_compiled_python(fname, last_dot))
      return EXCL_OPEN;

   if (!open_for_write(flags) && is_python(fname, last_dot))
      return REDIRECT;

   return ORIG_CALL;
}

int exec_filter(const char *fname)
{
   if (relocate_spindleapi())
      return REDIRECT;

   if (opts & OPT_RELOCEXEC)
      return REDIRECT;
   else
      return ORIG_CALL;
}

int stat_filter(const char *fname)
{
   char *last_dot, *last_slash;

   if (relocate_spindleapi())
      return REDIRECT;

   last_dot = strrchr(fname, '.');
   last_slash = strrchr(fname, '/');

   if (opts & OPT_RELOCJL) {
      if (is_julia(fname, last_dot))
         return REDIRECT;
      // TODO: DSO handling below is probably also wanted by Julia
   }

   if (!(opts & OPT_RELOCPY))
      return ORIG_CALL;

   if (is_python_path(fname))
      return REDIRECT;

   if (is_dso(fname, last_slash, last_dot) ||
       is_python(fname, last_dot) || 
       is_lib_prefix(fname, last_slash))
      return REDIRECT;
   else
      return ORIG_CALL;
}

int fd_filter(int fd)
{
   if (opts & OPT_NOHIDE)
      return ORIG_CALL;
   if (fd == -1)
      return ORIG_CALL;

   if (is_client_fd(ldcsid, fd))
      return ERR_CALL;
   if (is_debug_fd(fd))
      return ERR_CALL;

   return ORIG_CALL;
}
