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
   int result, fd, argc, interp_argc, i, j, errcode;
   char interpreter_line[MAX_PATH_LEN+1];
   char *interpreter, *new_interpreter;
   char **interpreter_args;
   *new_argv = NULL;

   if (argv && argv[0] && strcmp(argv[0], orig_path) != 0) {
      char *lastslash = strrchr(orig_path, '/');
      if (lastslash && strcmp(lastslash+1, argv[0]) != 0) {
         debug_printf2("Not treating %s as a script because it's argv[0] (%s) is different than the executable, "
                       "and Spindle can't emulate that\n", orig_path, argv[0]);
         return SCRIPT_NOTSCRIPT;
      }
   }
   if (opts & OPT_REMAPEXEC) {
      return SCRIPT_NOTSCRIPT;
   }
   
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
   get_relocated_file(ldcsid, interpreter, 1, &new_interpreter, &errcode, NULL);
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

   for (i = 0; i < interp_argc; i++)
      (*new_argv)[j++] = spindle_strdup(interpreter_args[i]);
   /* If argv[0] is not a path, replace with absolute path to mimic kernel behavior */
   (*new_argv)[j++] = (argv[0] && strchr(argv[0], '/')) ? argv[0] : orig_path;
   for (i = 1; i < argc; i++) {
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

int exec_pathsearch(int ldcsid, const char *orig_exec, char **reloc_exec, int *errcode)
{
   char *saveptr = NULL, *path, *cur;
   char newexec[MAX_PATH_LEN+1];
   int result;

   if (!orig_exec) {
      err_printf("Null exec passed to exec_pathsearch\n");
      *reloc_exec = NULL;
      return -1;
   }
   
   if (orig_exec[0] == '/' || orig_exec[0] == '.') {
      result = get_relocated_file(ldcsid, (char *) orig_exec, 1, reloc_exec, errcode, NULL);
      if (result == DISCONNECT) {
         debug_printf3("Disconnected during exec\n");
         return DISCONNECT;
      }
      debug_printf3("exec_pathsearch translated %s to %s\n", orig_exec, *reloc_exec);
      return 0;
   }

   path = getenv("PATH");
   if (!path) {
      result = get_relocated_file(ldcsid, (char *) orig_exec, 1, reloc_exec, errcode, NULL);
      if (result == DISCONNECT) {
         debug_printf3("Disconnected during exec\n");
         return DISCONNECT;
      }
      debug_printf3("No path.  exec_pathsearch translated %s to %s\n", orig_exec, *reloc_exec);
      return 0;
   }
   path = spindle_strdup(path);

   debug_printf3("exec_pathsearch using path %s on file %s\n", path, orig_exec);
   int found = 0;
   int access_denied_found = 0;
   for (cur = strtok_r(path, ":", &saveptr); cur; cur = strtok_r(NULL, ":", &saveptr)) {
      struct stat buf;
      int exists = 0;
      snprintf(newexec, MAX_PATH_LEN, "%s/%s", cur, orig_exec);
      newexec[MAX_PATH_LEN] = '\0';
      
      debug_printf2("Exec search operation requesting file via stat: %s\n", newexec);
      int result = get_stat_result(ldcsid, newexec, 0, &exists, &buf);
      if (result == STAT_SELF_OPEN) {
         result = stat(newexec, &buf);
         exists = (result != -1);
      }
      if (result == DISCONNECT) {
         debug_printf3("Disconnected during exec\n");
         if (path) spindle_free(path);
         return DISCONNECT;
      }
      if (!exists)
         continue;
      if (buf.st_mode & S_IFDIR) {
         debug_printf3("Skipping file %s in pathsearch: directory\n", newexec);
         access_denied_found = 1;         
         continue;
      }
      if (!(buf.st_mode & 0111)) {
         debug_printf3("Skipping file %s in pathsearch: not executable\n", newexec);
         access_denied_found = 1;
         continue;
      }
      debug_printf2("File %s exists and has execute set, requesting full file\n", newexec);
      result = get_relocated_file(ldcsid, newexec, 1, reloc_exec, errcode, NULL);
      if (result == DISCONNECT) {
         debug_printf3("Disconnected during exec\n");
         if (path) spindle_free(path);
         return DISCONNECT;
      }
      debug_printf2("Exec search request returned %s -> %s\n", newexec, *reloc_exec ? *reloc_exec : "NULL");
      if (*reloc_exec) {
         found = 1;
         break;
      }
      if (*errcode == EACCES) {
         *reloc_exec = spindle_strdup(newexec);
         found = 1;
         break;
      }
   }
   spindle_free(path);
   if (found)
      return 0;

   if (access_denied_found) {
      debug_printf3("Non executable file, setting errcode to %d\n", EACCES);
      *errcode = EACCES;
      return -1;
   }
   *errcode = ENOENT;
   return -1;
}

extern char **parse_colonsep_prefixes(char *colonsep_list, number_t number);
extern number_t number;
int get_dirlists(char ***prefixes, char ***eexecs)
{
   static char **local_prefixes = NULL;
   static char **exec_excludes = NULL;
   static int did_query = 0;
   char *local_str = NULL, *exec_str = NULL, *to_free = NULL;
   int result;

   if (did_query) {
      if (prefixes)
         *prefixes = local_prefixes;
      if (eexecs) 
         *eexecs = exec_excludes;
      return 0;
   }

   did_query = 1;
   result = send_dirlists_request(ldcsid, &local_str, &exec_str, &to_free);
   if (result == -1) {
      debug_printf("Returning error from get_dirlists because of send error\n");
      return -1;
   }

   local_prefixes = parse_colonsep_prefixes(local_str, number);
   exec_excludes = parse_colonsep_prefixes(exec_str, number);

   if (to_free)
      spindle_free(to_free);
   
   if (prefixes)
      *prefixes = local_prefixes;
   if (eexecs)
      *eexecs = exec_excludes;
   return result;
}

int isExecExcluded(const char *fname)
{
   const char *aout = NULL;
   const char *lastslash;
   char **exec_excludes = NULL;
   int i, result;

   if (!fname)
      return 0;

   result = get_dirlists(NULL, &exec_excludes);
   if (result == -1)
      return -1;
   if (!exec_excludes)
      return 0;
   
   lastslash = strrchr(fname, '/');
   if (lastslash) 
      aout = lastslash+1;
   else
      aout = fname;

   for (i = 0; exec_excludes[i] != NULL; i++) {
      if (strcmp(exec_excludes[i], aout) == 0) {
         return 1;
      }
   }
   return 0;
}

