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

#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>

#include "ldcs_api.h"
#include "client.h"
#include "client_heap.h"
#include "client_api.h"
#include "should_intercept.h"
#include "exec_util.h"

static int (*orig_execv)(const char *path, char *const argv[]);
static int (*orig_execve)(const char *path, char *const argv[], char *const envp[]);
static int (*orig_execvp)(const char *file, char *const argv[]);

static int prep_exec(const char *filepath, char **argv,
                     char *newname, char *newpath, int newpath_size,
                     char ***new_argv)
{
   int result;
   char *interp_name;

   if (!newname) {
      snprintf(newpath, newpath_size, "%s/%s", NOT_FOUND_PREFIX, filepath);
      newpath[newpath_size-1] = '\0';
      return 0;
   }

   result = adjust_if_script(filepath, newname, argv, &interp_name, new_argv);
   if (result == SCRIPT_NOTSCRIPT) {
      strncpy(newpath, newname, newpath_size);
      newpath[newpath_size - 1] = '\0';
      spindle_free(newname);
      return 0;
   }
   else if (result == SCRIPT_ERR || result == SCRIPT_ENOENT) {
      strncpy(newpath, newname, newpath_size);
      newpath[newpath_size - 1] = '\0';
      spindle_free(newname);
      return -1;
   }
   else if (result == 0) {
      // TODO: Mark filepath->newpath as future redirection for open
      strncpy(newpath, interp_name, newpath_size);
      newpath[newpath_size - 1] = '\0';
      return 0;
   }
   else {
      err_printf("Unknown return from adjust_if_script\n");
      return -1;
   }
}

static int find_exec(const char *filepath, char **argv, char *newpath, int newpath_size, char ***new_argv)
{
   char *newname = NULL;

   if (!filepath) {
      newpath[0] = '\0';
      return 0;
   }

   check_for_fork();
   if (ldcsid < 0 || !use_ldcs || exec_filter(filepath) != REDIRECT) {
      strncpy(newpath, filepath, newpath_size);
      newpath[newpath_size-1] = '\0';
      return 0;
   }

   sync_cwd();
   debug_printf2("Exec operation requesting file: %s\n", filepath);
   send_file_query(ldcsid, (char *) filepath, &newname);
   debug_printf("Exec file request returned %s -> %s\n", filepath, newname ? newname : "NULL");

   return prep_exec(filepath, argv, newname, newpath, newpath_size, new_argv);
}

static int find_exec_pathsearch(const char *filepath, char **argv, char *newpath, int newpath_size, char ***new_argv)
{
   char *newname = NULL;
   int result;

   if (!filepath) {
      newpath[0] = '\0';
      return 0;
   }
   if (filepath[0] == '/' || filepath[0] == '.') {
      find_exec(filepath, argv, newpath, newpath_size, new_argv);
      return 0;
   }
   check_for_fork();
   if (ldcsid < 0 || !use_ldcs) {
      strncpy(newpath, filepath, newpath_size);
      newpath[newpath_size-1] = '\0';
      return 0;
   }
   sync_cwd();

   result = exec_pathsearch(ldcsid, filepath, &newname);
   if (result == -1)
      return -1;

   result = prep_exec(filepath, argv, newname, newpath, newpath_size, new_argv);
   return result;
}

#define ARGV_MAX 1024
#define VARARG_TO_ARGV                                                  \
   char *initial_argv[ARGV_MAX];                                        \
   char **argv = initial_argv, **newp;                                  \
   char **new_argv = NULL;                                              \
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
    
#define VARARG_TO_ARGV_CLEANUP                                       \
   va_end(arglist);                                                  \
   if (argv != initial_argv) {                                       \
      spindle_free(argv);                                            \
   }                                                                 \
   if (new_argv) {                                                   \
      spindle_free(new_argv);                                        \
   }



static int execl_wrapper(const char *path, const char *arg0, ...)
{
   int error, result;
   char newpath[MAX_PATH_LEN+1];

   VARARG_TO_ARGV;

   debug_printf2("Intercepted execl on %s\n", path);

   find_exec(path, argv, newpath, MAX_PATH_LEN+1, &new_argv);
   debug_printf("execl redirection of %s to %s\n", path, newpath);
   result = execv(newpath, new_argv ? new_argv : argv);
   error = errno;

   VARARG_TO_ARGV_CLEANUP;

   set_errno(error);
   return result;
}

static int execv_wrapper(const char *path, char *const argv[])
{
   char newpath[MAX_PATH_LEN+1];
   char **new_argv = NULL;
   int result, error;

   debug_printf2("Intercepted execv on %s\n", path);
   find_exec(path, (char **) argv, newpath, MAX_PATH_LEN+1, &new_argv);
   debug_printf("execv redirection of %s to %s\n", path, newpath);
   result = orig_execv(newpath, new_argv ? new_argv : argv);
   error = errno;

   if (new_argv)
      spindle_free(new_argv);
   
   set_errno(error);
   return result;

}

static int execle_wrapper(const char *path, const char *arg0, ...)
{
   int error, result;
   char **envp;
   char newpath[MAX_PATH_LEN+1];

   VARARG_TO_ARGV;

   envp = va_arg(arglist, char **);
   debug_printf2("Intercepted execle on %s\n", path);
   find_exec(path, argv, newpath, MAX_PATH_LEN+1, &new_argv);
   debug_printf("execle redirection of %s to %s\n", path, newpath);
   result = execve(newpath, new_argv ? new_argv : argv, envp);
   error = errno;

   VARARG_TO_ARGV_CLEANUP;

   set_errno(error);
   return result;
}

static int execve_wrapper(const char *path, char *const argv[], char *const envp[])
{
   char newpath[MAX_PATH_LEN+1];
   char **new_argv = NULL;
   int result, error;

   debug_printf2("Intercepted execve on %s\n", path);
   find_exec(path, (char **) argv, newpath, MAX_PATH_LEN+1, &new_argv);
   debug_printf("execve redirection of %s to %s\n", path, newpath);
   result = orig_execve(newpath, new_argv ? new_argv : argv, (char **) envp);
   error = errno;
   if (new_argv)
      spindle_free(new_argv);
   set_errno(error);
   return result;
}

static int execlp_wrapper(const char *path, const char *arg0, ...)
{
   int error, result;
   char newpath[MAX_PATH_LEN+1];

   VARARG_TO_ARGV;
   debug_printf2("Intercepted execlp on %s\n", path);
   find_exec_pathsearch(path, argv, newpath, MAX_PATH_LEN+1, &new_argv);
   debug_printf("execlp redirection of %s to %s\n", path, newpath);
   result = execv(newpath, new_argv ? new_argv : argv);
   error = errno;

   VARARG_TO_ARGV_CLEANUP;

   set_errno(error);
   return result;
}

static int execvp_wrapper(const char *path, char *const argv[])
{
   char newpath[MAX_PATH_LEN+1];
   char **new_argv = NULL;
   int result, error;

   debug_printf2("Intercepted execvp on %s\n", path);
   find_exec_pathsearch(path, (char **) argv, newpath, MAX_PATH_LEN+1, &new_argv);
   debug_printf("execvp redirection of %s to %s\n", path, newpath);
   result = orig_execvp(newpath, new_argv ? new_argv : argv);
   error = errno;
   if (new_argv)
      spindle_free(new_argv);
   set_errno(error);
   return result;
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

pid_t vfork_wrapper()
{
   /* Spindle can't handle vforks */
   debug_printf("Translating vfork into fork\n");
   return fork();
}

ElfX_Addr redirect_fork(const char *symname, ElfX_Addr value)
{
   if (strcmp(symname, "vfork") == 0) {
      return (ElfX_Addr) vfork_wrapper;
   }
   else {
      debug_printf3("Not translating fork call %s\n", symname);
      return (ElfX_Addr) value;
   }
}
