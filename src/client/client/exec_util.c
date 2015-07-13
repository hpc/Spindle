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
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>

#include "exec_util.h"
#include "spindle_debug.h"
#include "client.h"
#include "client_heap.h"
#include "client_api.h"
#include "config.h"

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
         if (interp[i] == '\n') {
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

static int parse_interp_args(char *interp_line, char **interp_exec, char ***interp_args)
{
   char *eol, *c;
   unsigned int num_entries = 0, cur;
   for (eol = interp_line; *eol; eol++);

   for (c = interp_line; c != eol; c++) {
      if (*c == '\t' || *c == ' ') *c = '\0';
   }
   
   c = interp_line;
   for (;;) {
      while (*c == '\0' && c != eol) c++;
      if (c == eol) break;

      num_entries++;

      while (*c != '\0') c++;
      if (c == eol) break;
   }

   if (!num_entries) {
      err_printf("No interpreter name found\n");
      return -1;
   }

   *interp_args = (char **) spindle_malloc(sizeof(char *) * (num_entries + 1));
   
   c = interp_line;
   cur = 0;
   for (;;) {
      while (*c == '\0' && c != eol) c++;
      if (c == eol) break;

      (*interp_args)[cur] = c;
      cur++;

      while (*c != '\0') c++;
      if (c == eol) break;
   }
   (*interp_args)[cur] = NULL;
      
   *interp_exec = (*interp_args)[0];
   return 0;
}


int adjust_if_script(const char *orig_path, char *reloc_path, char **argv, char **interp_path, char ***new_argv)
{
   int result, fd, argc, interp_argc, i, j;
   char interpreter_line[MAX_PATH_LEN+1];
   char *interpreter, *new_interpreter;
   char **interpreter_args;
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
   
   result = read_script_interp(fd, reloc_path, interpreter_line, sizeof(interpreter_line));
   if (result == -1) {
      close(fd);
      return SCRIPT_ENOENT;
   }
   close(fd);
   debug_printf3("Interpreter line for script %s is %s\n", orig_path, interpreter_line);

   result = parse_interp_args(interpreter_line, &interpreter, &interpreter_args);
   if (result == -1) {
      return SCRIPT_ENOENT;
   }
   
   debug_printf2("Exec operation requesting interpreter %s for script %s\n", interpreter, orig_path);
   get_relocated_file(ldcsid, interpreter, &new_interpreter);
   debug_printf2("Changed interpreter %s to %s for script %s\n", 
                 interpreter, new_interpreter ? new_interpreter : "NULL", orig_path);
   if (!new_interpreter) {
      err_printf("Script interpreter %s does not exist in script %s\n", interpreter, orig_path);
      spindle_free(interpreter_args);
      return SCRIPT_ENOENT;
   }

   /* Count args on command line and interpreter line */
   for (argc = 0; argv[argc] != NULL; argc++);
   for (interp_argc = 0; interpreter_args[interp_argc] != NULL; interp_argc++);

   *new_argv = (char **) spindle_malloc(sizeof(char*) * (argc + interp_argc + 2));
   j = 0;

   (*new_argv)[j++] = new_interpreter;
   for (i = 1; i < interp_argc; i++)
      (*new_argv)[j++] = spindle_strdup(interpreter_args[i]);
   for (i = 0; i < argc; i++) {
      (*new_argv)[j++] = argv[i];
   }
   (*new_argv)[j++] = NULL;

   *interp_path = new_interpreter;
   debug_printf3("Rewritten interpreter cmdline is: ");
   for (i = 0; i<argc+1; i++) {
      bare_printf3("%s ", (*new_argv)[i]);
   }
   bare_printf3("\n");

   spindle_free(interpreter_args);

   return 0;
}

int exec_pathsearch(int ldcsid, const char *orig_exec, char **reloc_exec)
{
   char *saveptr = NULL, *path, *cur;
   char newexec[MAX_PATH_LEN+1];

   if (!orig_exec) {
      err_printf("Null exec passed to exec_pathsearch\n");
      *reloc_exec = NULL;
      return -1;
   }
   
   if (orig_exec[0] == '/' || orig_exec[0] == '.') {
      get_relocated_file(ldcsid, (char *) orig_exec, reloc_exec);
      debug_printf3("exec_pathsearch translated %s to %s\n", orig_exec, *reloc_exec);
      return 0;
   }

   path = getenv("PATH");
   if (!path) {
      get_relocated_file(ldcsid, (char *) orig_exec, reloc_exec);
      debug_printf3("No path.  exec_pathsearch translated %s to %s\n", orig_exec, *reloc_exec);
      return 0;
   }
   path = spindle_strdup(path);

   debug_printf3("exec_pathsearch using path %s on file %s\n", path, orig_exec);
   for (cur = strtok_r(path, ":", &saveptr); cur; cur = strtok_r(NULL, ":", &saveptr)) {
      snprintf(newexec, MAX_PATH_LEN, "%s/%s", cur, orig_exec);
      newexec[MAX_PATH_LEN] = '\0';

      debug_printf2("Exec search operation requesting file: %s\n", newexec);
      get_relocated_file(ldcsid, newexec, reloc_exec);
      debug_printf("Exec search request returned %s -> %s\n", newexec, *reloc_exec ? *reloc_exec : "NULL");
      if (*reloc_exec)
         break;
   }
   spindle_free(path);
   if (*reloc_exec)
      return 0;

   *reloc_exec = spindle_strdup(orig_exec);
   return 0;
}
