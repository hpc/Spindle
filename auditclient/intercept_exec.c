/*
  This file is part of Spindle.  For copyright information see the COPYRIGHT 
  file in the top level directory, or at 
  <TODO:URL>.

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

static int (*orig_execv)(const char *path, char *const argv[]);
static int (*orig_execve)(const char *path, char *const argv[], char *const envp[]);
static int (*orig_execvp)(const char *file, char *const argv[]);

static void find_exec(const char *filepath, char *newpath, int newpath_size)
{
   char *newname;

   if (!filepath) {
      newpath[0] = '\0';
      return;
   }
   check_for_fork();
   if (ldcsid < 0 || !use_ldcs) {
      strncpy(newpath, filepath, newpath_size);
      newpath[newpath_size-1] = '\0';
      return;
   }

   sync_cwd();
   debug_printf2("Exec operation requesting file: %s\n", filepath);
   send_file_query(ldcsid, (char *) filepath, &newname);
   debug_printf("Exec file request returned %s -> %s\n", filepath, newname ? newname : "NULL");
   if (newname) {
      strncpy(newpath, newname, newpath_size);
      spindle_free(newname);
   }
   else {
      snprintf(newpath, newpath_size, "%s/%s", NOT_FOUND_PREFIX, filepath);
   }
   newpath[newpath_size-1] = '\0';      
}

static void find_exec_pathsearch(const char *filepath, char *newpath, int newpath_size)
{
   char *newname = NULL, *path, *cur, *saveptr;
   char path_to_try[MAX_PATH_LEN+1];

   if (!filepath) {
      newpath[0] = '\0';
      return;
   }
   if (filepath[0] == '/') {
      find_exec(filepath, newpath, newpath_size);
      return;
   }
   check_for_fork();
   if (ldcsid < 0 || !use_ldcs) {
      strncpy(newpath, filepath, newpath_size);
      newpath[newpath_size-1] = '\0';
      return;
   }
   sync_cwd();

   path = getenv("PATH");
   if (!path) {
      strncpy(newpath, filepath, newpath_size);
      newpath[newpath_size-1] = '\0';
      return;
   }
   path = spindle_strdup(path);

   for (cur = strtok_r(path, ":", &saveptr); !cur && !newname; cur = strtok_r(NULL, ":", &saveptr)) {
      snprintf(path_to_try, MAX_PATH_LEN+1, "%s/%s", cur, filepath);
      debug_printf2("Exec search operation requesting file: %s\n", filepath);
      send_file_query(ldcsid, path_to_try, &newname);
      debug_printf("Exec search request returned %s -> %s\n", filepath, newname ? newname : "NULL");
   }
   if (newname) {
      strncpy(newpath, newname, newpath_size);
      spindle_free(newname);
   }
   else {
      snprintf(newpath, newpath_size, "%s/%s", NOT_FOUND_PREFIX, filepath);
   }
   spindle_free(path);
}

#define ARGV_MAX 1024
#define VARARG_TO_ARGV                                                  \
   char *initial_argv[ARGV_MAX];                                        \
   char **argv = initial_argv, **newp;                                  \
   va_list arglist;                                                     \
   int cur = 0;                                                         \
   int size = ARGV_MAX;                                                 \
                                                                        \
   va_start(arglist, arg0);                                             \
                                                                        \
   argv[cur] = (char *) arg0;                                           \
   while (argv[cur++] != NULL) {                                        \
      if (cur >= size) {                                                \
         size *= 2;                                                     \
         if (argv == initial_argv) {                                    \
            argv = (char **) spindle_malloc(sizeof(char *) * size);     \
            if (!argv) {                                                \
               errno = ENOMEM;                                          \
               return -1;                                               \
            }                                                           \
            memcpy(argv, initial_argv, ARGV_MAX*sizeof(char*));         \
         }                                                              \
         else {                                                         \
            newp = (char **) spindle_realloc(argv, sizeof(char *) * size); \
            if (!newp) {                                                \
               spindle_free(argv);                                      \
               errno = ENOMEM;                                          \
               return -1;                                               \
            }                                                           \
         }                                                              \
      }                                                                 \
                                                                        \
      argv[cur] = va_arg(arglist, char *);                              \
   }                                                                    \
    
#define VARARG_TO_ARGV_CLEANUP                  \
   va_end(arglist);                             \
   if (argv != initial_argv) {                  \
      spindle_free(argv);                       \
   }

static int execl_wrapper(const char *path, const char *arg0, ...)
{
   int error, result;
   char newpath[MAX_PATH_LEN+1];

   VARARG_TO_ARGV;

   debug_printf2("Intercepted execl on %s\n", path);

   find_exec(path, newpath, MAX_PATH_LEN+1);
   debug_printf("execl redirection of %s to %s\n", path, newpath);
   result = execv(newpath, argv);
   error = errno;

   VARARG_TO_ARGV_CLEANUP;

   set_errno(error);
   return result;
}

static int execv_wrapper(const char *path, char *const argv[])
{
   char newpath[MAX_PATH_LEN+1];

   debug_printf2("Intercepted execv on %s\n", path);
   find_exec(path, newpath, MAX_PATH_LEN+1);
   debug_printf("execv redirection of %s to %s\n", path, newpath);
   return orig_execv(newpath, argv);
}

static int execle_wrapper(const char *path, const char *arg0, ...)
{
   int error, result;
   char **envp;
   char newpath[MAX_PATH_LEN+1];

   VARARG_TO_ARGV;

   envp = va_arg(arglist, char **);
   debug_printf2("Intercepted execle on %s\n", path);
   find_exec(path, newpath, MAX_PATH_LEN+1);
   debug_printf("execle redirection of %s to %s\n", path, newpath);
   result = execve(newpath, argv, envp);
   error = errno;

   VARARG_TO_ARGV_CLEANUP;

   set_errno(error);
   return result;
}

static int execve_wrapper(const char *path, char *const argv[], char *const envp[])
{
   char newpath[MAX_PATH_LEN+1];
   debug_printf2("Intercepted execve on %s\n", path);
   find_exec(path, newpath, MAX_PATH_LEN+1);
   debug_printf("execve redirection of %s to %s\n", path, newpath);
   return orig_execve(newpath, argv, envp);
}

static int execlp_wrapper(const char *path, const char *arg0, ...)
{
   int error, result;
   char newpath[MAX_PATH_LEN+1];

   VARARG_TO_ARGV;
   debug_printf2("Intercepted execlp on %s\n", path);
   find_exec_pathsearch(path, newpath, MAX_PATH_LEN+1);
   debug_printf("execlp redirection of %s to %s\n", path, newpath);
   result = execv(newpath, argv);
   error = errno;

   VARARG_TO_ARGV_CLEANUP;

   set_errno(error);
   return result;
}

static int execvp_wrapper(const char *path, char *const argv[])
{
   char newpath[MAX_PATH_LEN+1];

   debug_printf2("Intercepted execvp on %s\n", path);
   find_exec_pathsearch(path, newpath, MAX_PATH_LEN+1);
   debug_printf("execve redirection of %s to %s\n", path, newpath);
   return orig_execvp(newpath, argv);
}

ElfX_Addr redirect_exec(const char *symname, ElfX_Addr value)
{
   if (strcmp(symname, "execl") == 0) {
      return (ElfX_Addr) execl_wrapper;
   }
   else if (strcmp(symname, "execv") == 0) {
      if (!orig_execv)
         orig_execv = (void *) value;
      return (ElfX_Addr) execv_wrapper;
   }
   else if (strcmp(symname, "execle") == 0) {
      return (ElfX_Addr) execle_wrapper;
   }
   else if (strcmp(symname, "execve") == 0) {
      if (!orig_execve)
         orig_execve = (void *) value;
      return (ElfX_Addr) execve_wrapper;
   }
   else if (strcmp(symname, "execlp") == 0) {
      return (ElfX_Addr) execlp_wrapper;
   }
   else if (strcmp(symname, "execvp") == 0) {
      if (!orig_execvp)
         orig_execvp = (void *) value;
      return (ElfX_Addr) execvp_wrapper;
   }
   else
      return (ElfX_Addr) value;
}
