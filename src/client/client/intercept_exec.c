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

#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <unistd.h>

#include "ldcs_api.h"
#include "client.h"
#include "client_heap.h"
#include "client_api.h"
#include "should_intercept.h"
#include "exec_util.h"
#include "handle_vararg.h"

#define INTERCEPT_EXEC
#if defined(INSTR_LIB)
#include "sym_alias.h"
#endif

int (*orig_execv)(const char *path, char *const argv[]);
int (*orig_execve)(const char *path, char *const argv[], char *const envp[]);
int (*orig_execvp)(const char *file, char *const argv[]);
pid_t (*orig_fork)();
extern void test_log(const char *name);

static int prep_exec(const char *filepath, char **argv,
                     char *newname, char *newpath, int newpath_size,
                     char ***new_argv, int errcode)
{
   int result;
   char *interp_name;

   debug_printf3("prep_exec for filepath %s to newpath %s\n", filepath, newpath);
   
   if (errcode == EACCES) {
      debug_printf2("exec'ing original path %s because file wasn't +r, but could be +x\n",
                    newpath);
      strncpy(newpath, filepath, newpath_size);
      newpath[newpath_size-1] = '\0';
      debug_printf("test_log(%s)\n", newpath);
      test_log(newpath);
      return 0;
   }
   if (errcode) {
      set_errno(errcode);
      return -1;
   }
   
   if (!newname) {
      snprintf(newpath, newpath_size, "%s/%s", NOT_FOUND_PREFIX, filepath);
      newpath[newpath_size-1] = '\0';
      return 0;
   }

   result = adjust_if_script(filepath, newname, argv, &interp_name, new_argv);
   if (opts & OPT_REMAPEXEC) {
      debug_printf2("exec'ing original path %s because we're running in remap mode\n", filepath);
      strncpy(newpath, filepath, newpath_size);
      newpath[newpath_size-1] = '\0';
      debug_printf("test_log(%s)\n", newname);
      test_log(newname);
      spindle_free(newname);
      return 0;
   }
   if (result == SCRIPT_NOTSCRIPT) {
      debug_printf2("exec'ing relocated path %s\n", newname);      
      strncpy(newpath, newname, newpath_size);
      newpath[newpath_size - 1] = '\0';
      spindle_free(newname);
      return 0;
   }
   else if (result == SCRIPT_ERR || result == SCRIPT_ENOENT) {
      strncpy(newpath, newname, newpath_size);
      newpath[newpath_size - 1] = '\0';
      debug_printf("test_log(%s)\n", newpath);      
      test_log(newpath);
      spindle_free(newname);
      return -1;
   }
   else if (result == 0) {
      // TODO: Mark filepath->newpath as future redirection for open
      strncpy(newpath, interp_name, newpath_size);
      newpath[newpath_size - 1] = '\0';
      debug_printf("test_log(%s)\n", newpath);      
      test_log(newpath);
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
   int errcode, exists;
   struct stat buf;

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
   debug_printf2("Requesting stat on exec of %s to validate file\n", filepath);
   get_stat_result(ldcsid, (char *) filepath, 0, &exists, &buf);
   if (!exists) {
      set_errno(ENOENT);
      return -1;
   } else if (!(buf.st_mode & 0111)) {
      set_errno(EACCES);
      return -1;
   }
   else if (buf.st_mode & S_IFDIR) {
      set_errno(EACCES);
      return -1;
   }
   debug_printf2("Exec operation requesting file: %s\n", filepath);
   get_relocated_file(ldcsid, (char *) filepath, &newname, &errcode);
   debug_printf("Exec file request returned %s -> %s with errcode %d\n",
                filepath, newname ? newname : "NULL", errcode);

   return prep_exec(filepath, argv, newname, newpath, newpath_size, new_argv, errcode);
}

static int find_exec_pathsearch(const char *filepath, char **argv, char *newpath, int newpath_size, char ***new_argv)
{
   char *newname = NULL;
   int result;
   int errcode;

   if (!filepath) {
      newpath[0] = '\0';
      return 0;
   }
   if (filepath[0] == '/' || filepath[0] == '.') {
      return find_exec(filepath, argv, newpath, newpath_size, new_argv);
   }
   check_for_fork();
   if (ldcsid < 0 || !use_ldcs) {
      strncpy(newpath, filepath, newpath_size);
      newpath[newpath_size-1] = '\0';
      return 0;
   }
   sync_cwd();

   result = exec_pathsearch(ldcsid, filepath, &newname, &errcode);
   if (result == -1) {
      set_errno(errcode);
      return -1;
   }

   return prep_exec(filepath, argv, newname, newpath, newpath_size, new_argv, errcode);
}

int execl_wrapper(const char *path, const char *arg0, ...)
{
   int error, result;
   char newpath[MAX_PATH_LEN+1];

   VARARG_TO_ARGV;

   debug_printf2("Intercepted execl on %s\n", path);

   result = find_exec(path, argv, newpath, MAX_PATH_LEN+1, &new_argv);
   if (result == -1) {
      debug_printf("execl redirection of %s returning error code\n", path);
      return result;
   }
   debug_printf("execl redirection of %s to %s\n", path, newpath);
   if (orig_execv)
      result = orig_execv(newpath, new_argv ? new_argv : argv);
   else
      result = execv(newpath, new_argv ? new_argv : argv);
   error = errno;

   VARARG_TO_ARGV_CLEANUP;

   if (!orig_execv)
      set_errno(error);
   return result;
}

int execv_wrapper(const char *path, char *const argv[])
{
   char newpath[MAX_PATH_LEN+1];
   char **new_argv = NULL;
   int result;

   debug_printf2("Intercepted execv on %s\n", path);
   result = find_exec(path, (char **) argv, newpath, MAX_PATH_LEN+1, &new_argv);
   if (result == -1) {
      debug_printf("execv redirection of %s returning error code\n", path);      
      return result;
   }
   debug_printf("execv redirection of %s to %s\n", path, newpath);
   result = orig_execv(newpath, new_argv ? new_argv : argv);

   if (new_argv)
      spindle_free(new_argv);
   
   return result;

}

int execle_wrapper(const char *path, const char *arg0, ...)
{
   int error, result;
   char **envp;
   char newpath[MAX_PATH_LEN+1];

   VARARG_TO_ARGV;

   envp = va_arg(arglist, char **);
   debug_printf2("Intercepted execle on %s\n", path);
   result = find_exec(path, argv, newpath, MAX_PATH_LEN+1, &new_argv);
   if (result == -1) {
      debug_printf("execle redirection of %s returning error code\n", path);      
      return result;
   }
   debug_printf("execle redirection of %s to %s\n", path, newpath);
   if (orig_execve)
      result = orig_execve(newpath, new_argv ? new_argv : argv, envp);
   else
      result = execve(newpath, new_argv ? new_argv : argv, envp);
   error = errno;

   VARARG_TO_ARGV_CLEANUP;

   if (!orig_execve)
      set_errno(error);
   return result;
}

int execve_wrapper(const char *path, char *const argv[], char *const envp[])
{
   char newpath[MAX_PATH_LEN+1];
   char **new_argv = NULL;
   int result;

   debug_printf2("Intercepted execve on %s\n", path);
   result = find_exec(path, (char **) argv, newpath, MAX_PATH_LEN+1, &new_argv);
   if (result == -1) {
      debug_printf("execve redirection of %s returning error code\n", path);      
      return result;
   }   
   debug_printf2("execve redirection of %s to %s\n", path, newpath);
   result = orig_execve(newpath, new_argv ? new_argv : argv, (char **) envp);
   if (new_argv)
      spindle_free(new_argv);
   return result;
}

int execlp_wrapper(const char *path, const char *arg0, ...)
{
   int error, result;
   char newpath[MAX_PATH_LEN+1];

   VARARG_TO_ARGV;
   debug_printf2("Intercepted execlp on %s\n", path);
   result = find_exec_pathsearch(path, argv, newpath, MAX_PATH_LEN+1, &new_argv);
   if (result == -1) {
      debug_printf("execlp redirection of %s returning error code\n", path);      
      return result;
   }   
   debug_printf2("execlp redirection of %s to %s\n", path, newpath);
   if (orig_execv)
      result = orig_execv(newpath, new_argv ? new_argv : argv);
   else
      result = execv(newpath, new_argv ? new_argv : argv);
   error = errno;

   VARARG_TO_ARGV_CLEANUP;

   if (!orig_execv)
      set_errno(error);
   return result;
}

int execvp_wrapper(const char *path, char *const argv[])
{
   char newpath[MAX_PATH_LEN+1];
   char **new_argv = NULL;
   int result;

   debug_printf2("problem Intercepted execvp of %s\n", path);
   result = find_exec_pathsearch(path, (char **) argv, newpath, MAX_PATH_LEN+1, &new_argv);
   if (result == -1) {
      debug_printf("execvp redirection of %s returning error code\n", path);      
      return result;
   }   
   debug_printf("execvp redirection of %s to %s\n", path, newpath);
   
   result = orig_execvp(newpath, new_argv ? new_argv : argv);
   if (new_argv)
      spindle_free(new_argv);
   return result;
}

pid_t vfork_wrapper()
{
   /* Spindle can't handle vforks */
   debug_printf("Translating vfork into fork\n");
   if (orig_fork)
      return orig_fork();
   else
      return fork();
}

