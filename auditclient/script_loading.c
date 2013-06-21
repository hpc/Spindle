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
#include <errno.h>
#include <fcntl.h>

#include "script_loading.h"
#include "spindle_debug.h"
#include "client.h"
#include "client_heap.h"
#include "client_api.h"

static int is_script(int fd, char *path)
{
   int result;
   char header[2];

   do {
      result = read(fd, header, 2);
   } while (result == -1 && errno == EINTR);
   if (result == -1) {
      err_printf("Unable to read from file %s to test for script: %s\n", path, strerror(errno));
      return -1;
   }
   if (result != 2) {
      err_printf("Failed to read the correct number of bytes when testing %s for script\n", path);
      return -1;
   }
   
   if (header[0] != '#' || header[1] != '!') {
      debug_printf3("Determined exec target %s is not a script\n", path);
      return 0;
   }
   debug_printf("Exec target %s is a script\n", path);
   return 1;
}

static int read_script_interp(int fd, const char *path, char *interp, int interp_size)
{
   int result;
   int pos = 0, i;

   while (pos < interp_size) {
      result = read(fd, interp + pos, interp_size - pos);
      if (result == -1) {
         err_printf("Error reading interpreter name from script %s\n", path);
         return -1;
      }
      if (result == 0) {
         err_printf("Encountered EOF while reading interpreter name from script %s\n", path);
         return -1;
      }
      for (i = pos; i < result + pos; i++) {
         if (interp[i] == '\n' || interp[i] == '\t' || interp[i] == '\n') {
            interp[i] = '\0';
            return 0;
         }
      }
      pos += result;
   }
   err_printf("Interpreter file path in script %s was longer than the max path length %u\n",
               path, interp_size);
   return -1;
}

int adjust_if_script(const char *orig_path, char *reloc_path, char **argv, char **interp_path, char ***new_argv)
{
   int result, fd, argc, i;
   char interpreter[MAX_PATH_LEN+1];
   char *new_interpreter;
   *new_argv = NULL;

   fd = open(reloc_path, O_RDONLY);
   if (fd == -1) {
      err_printf("Unable to open file %s to test for script: %s\n", reloc_path, strerror(errno));
      return SCRIPT_ERR;
   }

   result = is_script(fd, reloc_path);
   if (result == -1) {
      close(fd);
      return SCRIPT_ENOENT;
   }
   if (result == 0) {
      close(fd);
      return SCRIPT_NOTSCRIPT;
   }
   
   result = read_script_interp(fd, reloc_path, interpreter, sizeof(interpreter));
   if (result == -1) {
      close(fd);
      return SCRIPT_ENOENT;
   }
   close(fd);
   
   debug_printf2("Exec operation requesting interpreter %s for script %s\n", interpreter, orig_path);
   send_file_query(ldcsid, interpreter, &new_interpreter);
   debug_printf2("Changed interpreter %s to %s for script %s\n", 
                 interpreter, new_interpreter ? new_interpreter : "NULL", orig_path);
   if (!new_interpreter) {
      err_printf("Script interpreter %s does not exist in script %s\n", interpreter, orig_path);
      return SCRIPT_ENOENT;
   }

   for (argc = 0; argv[argc] != NULL; argc++);
   *new_argv = (char **) spindle_malloc(sizeof(char*) * (argc + 2));
   (*new_argv)[0] = new_interpreter;
   for (i = 0; i < argc; i++) {
      (*new_argv)[i+1] = argv[i];
   }
   (*new_argv)[argc+1] = NULL;

   *interp_path = new_interpreter;
   debug_printf3("Rewritten interpreter cmdline is: ");
   for (i = 0; i<argc+1; i++) {
      bare_printf3("%s ", (*new_argv)[i]);
   }
   bare_printf3("\n");

   return 0;
}
