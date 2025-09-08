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
int (*orig_execl)(const char *path, const char *arg, ...);
int (*orig_execlp)(const char *path, const char *arg, ...);
int (*orig_execle)(const char *path, const char *arg, ...);
int (*orig_execvpe)(const char *file, char *const argv[], char *const envp[]);

pid_t (*orig_fork)();
pid_t (*orig_vfork)();

char* (*orig_getenv)(const char *name);
int (*orig_setenv)(const char *name, const char *value, int overwrite);
int (*orig_unsetenv)(const char *name);

extern void test_log(const char *name);

static int strIsPrefix(char *prefix, char *str)
{
   return (strncmp(prefix, str, strlen(prefix)) == 0);
}

static int shouldPropogateSpindle(char **envp, const char *fname)
{
   int i;
   char *spindle;

   if (isExecExcluded(fname)){
      debug_printf2("Not propogating spindle because %s is excluded.\n", fname);
      return 0;
   }
   
   spindle = orig_getenv ? orig_getenv("SPINDLE") : getenv("SPINDLE");
   if (spindle && (strcasecmp(spindle, "false") == 0 || strcmp("spindle", "0") == 0)) {
      debug_printf2("Not propogating spindle through exec because getenv(\"SPINDLE\") = %s\n", spindle);
      return 0;
   }

   if (envp) {
      for (i = 0; envp[i] != NULL; i++) {
         if (strIsPrefix("SPINDLE=", envp[i])) {
            spindle = envp[i] + strlen("SPINDLE=");
            if (strcasecmp(spindle, "false") == 0 || strcmp("spindle", "0") == 0) {
               debug_printf2("Not propogating spindle through exec because envp[\"SPINDLE\"] = %s\n", spindle);               
               return 0;
            }
            break;
         }
      }
   }

   debug_printf2("Decided to propogate spindle through exec\n");
   return 1;
}

static char *allocEnvAssignmentStr(const char *name, char *value)
{
   int len;
   char *str;
   len = strlen(name) + strlen(value) + 2;
   str = (char *) malloc(len);
   snprintf(str, len, "%s=%s", name, value);
   return str;
}

static int envpContains(char *const envp[], char *variable)
{
   int varlen = strlen(variable);
   char *const *e;
   for (e = envp; *e != NULL; e++) {
      if ((strncmp(*e, variable, varlen) == 0) && ((*e)[varlen] == '='))
         return 1;
   }
   return 0;
}

static void propogateEnvironmentStr(char *const orig_envp[], char **new_envp, int *pos, char *var)
{
   char *value;

   value = orig_getenv ? orig_getenv(var) : getenv(var);
   if (!value)
      return;
   if (envpContains(orig_envp, var))
      return;
   debug_printf3("Adding environment variable %s to env\n", var);
   new_envp[*pos] = allocEnvAssignmentStr(var, value);
   (*pos)++;
}

static char **removeEnvironmentStrs(char **envp)
{
   int num_envs, i, j;
   char **newenv;

   for (num_envs = 0; envp[num_envs] != NULL; num_envs++);
   newenv = malloc(sizeof(char *) * (num_envs+1));

   for (i = 0, j = 0; envp[i] != NULL; i++) {
      if (strIsPrefix("SPINDLE=", envp[i]))
         continue;
      if (strIsPrefix("LD", envp[i])) {
         if (strIsPrefix("LD_AUDIT=", envp[i]) ||
             strIsPrefix("LDCS_LOCATION=", envp[i]) ||
             strIsPrefix("LDCS_ORIG_LOCATION=", envp[i]) ||
             strIsPrefix("LDCS_CONNECTION=", envp[i]) ||
             strIsPrefix("LDCS_RANKINFO=", envp[i]) ||
             strIsPrefix("LDCS_OPTIONS=", envp[i]) ||
             strIsPrefix("LDCS_CACHESIZE=", envp[i]) ||
             strIsPrefix("LDCS_NUMBER=", envp[i])) {
            debug_printf3("Removing %s from exec environment\n", envp[i]);
            continue;
         }
      }
      newenv[j++] = (char *) envp[i];
   }
   newenv[j] = NULL;
   return newenv;
}

static char **updateEnvironment(char **envp, int *num_modified, int propogate_spindle)
{
   int orig_size = 0, new_size, pos = 0;
   char **cur, **newenv;
   char *empty_env[1];
   empty_env[0] = NULL;

   if (num_modified && !envp) {
      envp = empty_env;
   }

   if (!propogate_spindle) {
      if (!envp) {
         debug_printf2("Removing spindle from environment by unsetenv\n");
         int (*unsetf)(const char *);
         unsetf = orig_unsetenv ? orig_unsetenv : unsetenv;
         unsetf("SPINDLE");
         unsetf("LD_AUDIT");
         unsetf("LDCS_LOCATION");
         unsetf("LDCS_ORIG_LOCATION");
         unsetf("LDCS_CONNECTION");
         unsetf("LDCS_RANKINFO");
         unsetf("LDCS_OPTIONS");
         unsetf("LDCS_CACHESIZE");
         unsetf("LDCS_NUMBER");
         newenv = NULL;
         if (num_modified)
            *num_modified = 0;
      }
      else {
         debug_printf2("Removing spindle from environment by stripping it from envp list\n");         
         newenv = removeEnvironmentStrs(envp);
         *num_modified = -1;
      }
      return newenv;
   }

   if (envp) {
      debug_printf2("Propogating spindle environment by copying it to new envp list\n");
      for (cur = (char **) envp; *cur; cur++, orig_size++);
      new_size = orig_size + 20;
      newenv = (char **) malloc(new_size * sizeof(char*));
      
      propogateEnvironmentStr(envp, newenv, &pos, "SPINDLE");   
      propogateEnvironmentStr(envp, newenv, &pos, "LD_AUDIT");
      propogateEnvironmentStr(envp, newenv, &pos, "LDCS_LOCATION");
      propogateEnvironmentStr(envp, newenv, &pos, "LDCS_ORIG_LOCATION");
      propogateEnvironmentStr(envp, newenv, &pos, "LDCS_CACHEPATH");
      propogateEnvironmentStr(envp, newenv, &pos, "LDCS_CONNECTION");
      propogateEnvironmentStr(envp, newenv, &pos, "LDCS_RANKINFO");
      propogateEnvironmentStr(envp, newenv, &pos, "LDCS_OPTIONS");
      propogateEnvironmentStr(envp, newenv, &pos, "LDCS_CACHESIZE");
      propogateEnvironmentStr(envp, newenv, &pos, "LDCS_NUMBER");
      *num_modified = pos;
      for (cur = (char **) envp; *cur; cur++) {
         newenv[pos++] = *cur;
      }
      newenv[pos++] = NULL;
      assert(pos <= new_size);
      return newenv;
   }
   else {
      debug_printf2("Propogating spindle environment by leaving it set\n");
      if (num_modified)
         *num_modified = 0;
      return NULL;
   }

   return newenv;   
}

static void cleanEnvironment(char **envp, int num_modified) {
   int i;
   if (!envp)
      return;
   if (!num_modified)
      return;
   if (num_modified == -1) {
      free(envp);
      return;
   }
   
   for (i = 0; i < num_modified; i++) {
      free(envp[i]);
   }
   free(envp);
}

static int prep_exec(const char *filepath, char **argv,
                     char *newname, char *newpath, int newpath_size,
                     char ***new_argv, int errcode)
{
   int result;
   char *interp_name;
   int i;

   debug_printf3("prep_exec for filepath %s to newpath %s\n", filepath, newpath);
   if (spindle_debug_prints >= 3) {
      debug_printf3("Args for exec are:\n");
      for (i = 0; argv[i]; i++) {
         debug_printf3("%d. %s\n", i, argv[i]);
      }
   }
   
   
   if (errcode == EACCES) {
      strncpy(newpath, filepath, newpath_size);
      newpath[newpath_size-1] = '\0';
      debug_printf2("exec'ing original path %s because file wasn't +r, but could be +x\n",
                    newpath);
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

static int find_exec(const char *filepath, char **argv, char *newpath, int newpath_size, char ***new_argv, char **envp, int *propogate_spindle)
{
   char *newname = NULL;
   int errcode, exists;
   struct stat buf;
   int reloc_exec;

   *propogate_spindle = shouldPropogateSpindle(envp, filepath);

   if (!filepath) {
      newpath[0] = '\0';
      return 0;
   }

   check_for_fork();
   if (ldcsid < 0 || !use_ldcs || exec_filter(filepath) != REDIRECT || !*propogate_spindle) {
      reloc_exec = 0;
   }
   else {
      reloc_exec = 1;
   }

   if (!reloc_exec) {
      debug_printf2("Exec operation passing through original file %s\n", filepath);
      strncpy(newpath, filepath, newpath_size);
      newpath[newpath_size-1] = '\0';
      return 0;
   }

   sync_cwd();
   
   debug_printf2("Requesting stat on exec of %s to validate file\n", filepath);
   int result = get_stat_result(ldcsid, (char *) filepath, 0, &exists, &buf);
   if (result == STAT_SELF_OPEN) {
      result = stat(filepath, &buf);
      exists = (result != -1);
   }
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
   get_relocated_file(ldcsid, (char *) filepath, 1, &newname, &errcode, NULL);
   debug_printf("Exec file request returned %s -> %s with errcode %d\n",
                filepath, newname ? newname : "NULL", errcode);
       
   return prep_exec(filepath, argv, newname, newpath, newpath_size, new_argv, errcode);
}

static int find_exec_pathsearch(const char *filepath, char **argv, char *newpath, int newpath_size, char ***new_argv, char **envp, int *propogate_spindle)
{
   char *newname = NULL;
   int result;
   int errcode;
   int reloc_exec;

   *propogate_spindle = shouldPropogateSpindle(envp, filepath);

   if (!filepath) {
      newpath[0] = '\0';
      return 0;
   }
   if (filepath[0] == '/' || filepath[0] == '.') {
      return find_exec(filepath, argv, newpath, newpath_size, new_argv, envp, propogate_spindle);
   }

   check_for_fork();
   if (ldcsid < 0 || !use_ldcs || !*propogate_spindle) {
      reloc_exec = 0;
   }
   else {
      reloc_exec = 1;
   }

   if (!reloc_exec) {
      debug_printf2("Exec operation passing through original file %s\n", filepath);
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
   debug_printf("Exec file request returned %s -> %s with errcode %d\n",
                filepath, newname ? newname : "NULL", errcode);

   return prep_exec(filepath, argv, newname, newpath, newpath_size, new_argv, errcode);
}

int execl_wrapper(const char *path, const char *arg0, ...)
{
   int error, result, propogate_spindle;
   char newpath[MAX_PATH_LEN+1];

   VARARG_TO_ARGV;

   debug_printf2("Intercepted execl on %s\n", path);

   result = find_exec(path, argv, newpath, MAX_PATH_LEN+1, &new_argv, NULL, &propogate_spindle);
   if (result == -1) {
      debug_printf("execl redirection of %s returning error code\n", path);
      return result;
   }

   updateEnvironment(NULL, NULL, propogate_spindle);
   debug_printf("execl redirection of %s to %s\n", path, newpath);
   if (orig_execv) {
      result = orig_execv(newpath, new_argv ? new_argv : argv);
   }
   else {
      result = execv(newpath, new_argv ? new_argv : argv);
   }
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
   int result, propogate_spindle;

   debug_printf2("Intercepted execv on %s\n", path);
   result = find_exec(path, (char **) argv, newpath, MAX_PATH_LEN+1, &new_argv, NULL, &propogate_spindle);
   if (result == -1) {
      debug_printf("execv redirection of %s returning error code\n", path);      
      return result;
   }
   updateEnvironment(NULL, NULL, propogate_spindle);   
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
   char **new_envp = NULL;
   char newpath[MAX_PATH_LEN+1];
   int propogate_spindle, env_modified = 0;

   VARARG_TO_ARGV;

   envp = va_arg(arglist, char **);
   debug_printf2("Intercepted execle on %s\n", path);
   result = find_exec(path, argv, newpath, MAX_PATH_LEN+1, &new_argv, envp, &propogate_spindle);
   if (result == -1) {
      debug_printf("execle redirection of %s returning error code\n", path);      
      return result;
   }

   new_envp = updateEnvironment((char **) envp, &env_modified, propogate_spindle);

   if (orig_execve)
      result = orig_execve(newpath, new_argv ? new_argv : argv, new_envp);
   else
      result = execve(newpath, new_argv ? new_argv : argv, new_envp);
   error = errno;

   VARARG_TO_ARGV_CLEANUP;
   cleanEnvironment(new_envp, env_modified);
   if (!orig_execve)
      set_errno(error);
   return result;
}

int execve_wrapper(const char *path, char *const argv[], char *const envp[])
{
   char newpath[MAX_PATH_LEN+1];
   char **new_argv = NULL;
   char **new_envp = NULL;
   int propogate_spindle, env_modified;
   int result;

   debug_printf2("Intercepted execve on %s\n", path);
   result = find_exec(path, (char **) argv, newpath, MAX_PATH_LEN+1, &new_argv, (char **) envp, &propogate_spindle);
   if (result == -1) {
      debug_printf("execve redirection of %s returning error code\n", path);
      return result;
   }
   new_envp = updateEnvironment((char **) envp, &env_modified, propogate_spindle);
   debug_printf2("execve redirection of %s to %s\n", path, newpath);
   result = orig_execve(newpath, new_argv ? new_argv : argv, new_envp);
   if (new_argv)
      spindle_free(new_argv);
   cleanEnvironment(new_envp, env_modified);
   return result;
}

int execlp_wrapper(const char *path, const char *arg0, ...)
{
   int error, result;
   char newpath[MAX_PATH_LEN+1];
   int propogate_spindle;

   VARARG_TO_ARGV;
   debug_printf2("Intercepted execlp on %s\n", path);
   result = find_exec_pathsearch(path, argv, newpath, MAX_PATH_LEN+1, &new_argv, NULL, &propogate_spindle);
   if (result == -1) {
      debug_printf("execlp redirection of %s returning error code\n", path);      
      return result;
   }
   updateEnvironment(NULL, NULL, propogate_spindle);
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
   int propogate_spindle;

   debug_printf2("Intercepted execvp of %s\n", path);
   result = find_exec_pathsearch(path, (char **) argv, newpath, MAX_PATH_LEN+1, &new_argv, NULL, &propogate_spindle);
   if (result == -1) {
      debug_printf("execvp redirection of %s returning error code\n", path);      
      return result;
   }   
   debug_printf("execvp redirection of %s to %s\n", path, newpath);

   updateEnvironment(NULL, NULL, propogate_spindle);   
   result = orig_execvp(newpath, new_argv ? new_argv : argv);
   if (new_argv)
      spindle_free(new_argv);
   return result;
}

int execvpe_wrapper(const char *path, char *const argv[], char *const envp[])
{
   char newpath[MAX_PATH_LEN+1];
   char **new_argv = NULL;
   char **new_envp = NULL;
   int propogate_spindle, env_modified;
   int result;

   debug_printf2("Intercepted execvpe on %s\n", path);
   result = find_exec_pathsearch(path, (char **) argv, newpath, MAX_PATH_LEN+1, &new_argv, (char **) envp, &propogate_spindle);
   if (result == -1) {
      debug_printf("execvpe redirection of %s returning error code\n", path);
      return result;
   }
   new_envp = updateEnvironment((char **) envp, &env_modified, propogate_spindle);
   debug_printf2("execvpe redirection of %s to %s\n", path, newpath);
   result = orig_execvpe(newpath, new_argv ? new_argv : argv, new_envp);
   if (new_argv)
      spindle_free(new_argv);
   cleanEnvironment(new_envp, env_modified);
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

