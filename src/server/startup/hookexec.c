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
#include <elf.h>
#include <link.h>
#include <string.h>
#include <stdio.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>

#include "spindle_launch.h"
#include "spindle_debug.h"
#include "handle_vararg.h"
#include "parse_plt.h"

static int cached_spindle_argc;
static char **cached_spindle_argv;
static char *cached_execfilter;
static int (*orig_execv)(const char *path, char *const argv[]);
static int (*orig_execve)(const char *path, char *const argv[], char *const envp[]);
static int (*orig_execvp)(const char *file, char *const argv[]);
static int execl_wrapper(const char *path, const char *arg0, ...);
static int execv_wrapper(const char *path, char *const argv[]);
static int execle_wrapper(const char *path, const char *arg0, ...);
static int execve_wrapper(const char *path, char *const argv[], char *const envp[]);
static int execlp_wrapper(const char *path, const char *arg0, ...);
static int execvp_wrapper(const char *path, char *const argv[]);

static int interceptExecForMap(struct link_map *map);
static int translateArgv(const char *orig_path, char **orig_argv, char ***modified_argv);
static void cleanupArgv(char **modified_argv);

#define spindle_malloc malloc
#define spindle_free free
#define spindle_realloc realloc

int spindleHookSpindleArgsIntoExecBE(int spindle_argc, char **spindle_argv, char *execfilter)
{
   static int wrapping_needed = 1;
   struct link_map *mapi;
   int result;

   debug_printf("User asked for spindle hooks on exec with argc = %d, execfilter = %s\n", spindle_argc, execfilter);
   
   cached_spindle_argc = spindle_argc;
   cached_spindle_argv = spindle_argv;
   cached_execfilter = execfilter;

   if (wrapping_needed != 1)
      goto done;

   orig_execv = dlsym(RTLD_DEFAULT, "execv");
   orig_execve = dlsym(RTLD_DEFAULT, "execve");
   orig_execvp = dlsym(RTLD_DEFAULT, "execvp");
   if (!orig_execv || !orig_execve || !orig_execvp) {
      err_printf("Could not find one of execv, execve, execvp (%p, %p, %p) with dlsym search. "
                 "Aborting exec hooking\n", orig_execv, orig_execve, orig_execvp);
      wrapping_needed = -1;
      goto done;
   }
   
   for (mapi = _r_debug.r_map; mapi; mapi = mapi->l_next) {
      result = interceptExecForMap(mapi);
      if (result == -1) {
         err_printf("Could not wrap exec's of %s.  Aborting exec hooking\n",
                    mapi->l_name && *mapi->l_name ? mapi->l_name : "[NONAME]");
         wrapping_needed = -1;
         goto done;
      }
   }
   wrapping_needed = 0;

  done:
   return wrapping_needed;
}

#define REMAP_FOR_EXEC(SYM, NAME, OFFSET) {        \
      if (SYM->st_shndx != SHN_UNDEF)              \
         continue;                                 \
      if (!NAME)                                   \
         continue;                                 \
      if (NAME[0] != 'e' && NAME[1] != 'x' &&      \
          NAME[2] != 'e' && NAME[3] != 'c')        \
         continue;                                 \
      if (strcmp(NAME, "execl") == 0)              \
         target = (void*) execl_wrapper;           \
      else if (strcmp(NAME, "execv") == 0)         \
         target = (void*) execv_wrapper;           \
      else if (strcmp(NAME, "execle") == 0)        \
         target = (void*) execle_wrapper;          \
      else if (strcmp(NAME, "execve") == 0)        \
         target = (void*) execve_wrapper;          \
      else if (strcmp(NAME, "execlp") == 0)        \
         target = (void*) execlp_wrapper;          \
      else if (strcmp(NAME, "execvp") == 0)        \
         target = (void*) execvp_wrapper;          \
      else                                         \
         continue;                                 \
      gotaddr = (void **) (OFFSET + map->l_addr);  \
      ASSIGN_FPTR(gotaddr, target);                \
   }

static int interceptExecForMap(struct link_map *map)
{
   void *target, **gotaddr;
   
   FOR_EACH_PLTREL(map, REMAP_FOR_EXEC);
   return 0;
}

static int translateArgv(const char *orig_path, char **orig_argv, char ***modified_argv)
{
   int new_argc, orig_argc;
   char **argv;
   int i, j = 0;

   *modified_argv = NULL;
   if (!orig_path) {
      debug_printf3("Warning: Translating argv for interception has NULL argv[0]\n");
      return -1;
   }
   if (cached_execfilter && strstr(orig_path, cached_execfilter) == NULL) {
      debug_printf3("Not intercepting exec of %s because it doesn't match %s\n", orig_path, cached_execfilter);
      return -1;
   }

   for (orig_argc = 0; orig_argv[orig_argc]; orig_argc++);
   new_argc = cached_spindle_argc + orig_argc;
   argv = (char **) malloc(sizeof(char*) * (new_argc+1));

   for (i = 0; i < cached_spindle_argc; i++) {
      if (cached_spindle_argv[i])
         argv[j++] = (char *) cached_spindle_argv[i];
   }
   for (i = 0; orig_argv[i]; i++) {
      argv[j++] = (char *) orig_argv[i];
   }
   argv[j] = NULL;

   if (spindle_debug_prints >= 2) {
      debug_printf2("Exec'ing:");
      for (i = 0; argv[i]; i++) {
         bare_printf2(" %s", argv[i]);
      }
      bare_printf2("\n");
   }   
   
   *modified_argv = argv;
   return 0;
}

static void cleanupArgv(char **modified_argv)
{
   if (!modified_argv)
      return;
   free(modified_argv);
}

static int execl_wrapper(const char *path, const char *arg0, ...)
{
   int error, result;
   char **modified_argv;

   VARARG_TO_ARGV;

   debug_printf2("Server intercepted execl on %s\n", path);
   result = translateArgv(path, argv, &modified_argv);
   if (result == -1)
      result = orig_execv(path, argv);
   else
      result = orig_execv(modified_argv[0], modified_argv);
   error = errno;

   VARARG_TO_ARGV_CLEANUP;
   cleanupArgv(modified_argv);
   errno = error;
   
   return result;
}

static int execv_wrapper(const char *path, char *const argv[])
{
   int error, result;
   char **modified_argv;

   debug_printf2("Server intercepted execv on %s\n", path);
   result = translateArgv(path, (char **) argv, &modified_argv);
   if (result == -1)
      result = orig_execv(path, argv);
   else
      result = orig_execv(modified_argv[0], modified_argv);
   error = errno;
   cleanupArgv(modified_argv);
   errno = error;
   return result;
}

static int execle_wrapper(const char *path, const char *arg0, ...)
{
   char **envp;
   int error, result;
   char **modified_argv;

   VARARG_TO_ARGV;
   envp = va_arg(arglist, char **);
   
   debug_printf2("Server intercepted execle on %s\n", path);
   result = translateArgv(path, argv, &modified_argv);
   if (result == -1)
      result = orig_execve(path, argv, envp);
   else
      result = orig_execve(modified_argv[0], modified_argv, envp);
   error = errno;

   VARARG_TO_ARGV_CLEANUP;
   cleanupArgv(modified_argv);
   errno = error;
   
   return result;
}

static int execve_wrapper(const char *path, char *const argv[], char *const envp[])
{
   int error, result;
   char **modified_argv;

   debug_printf2("Server intercepted execve on %s\n", path);
   result = translateArgv(path, (char **) argv, &modified_argv);
   if (result == -1)
      result = orig_execve(path, argv, envp);
   else
      result = orig_execve(modified_argv[0], modified_argv, envp);
   error = errno;
   cleanupArgv(modified_argv);
   errno = error;
   return result;
}

static int execlp_wrapper(const char *path, const char *arg0, ...)
{
   int error, result;
   char **modified_argv;

   VARARG_TO_ARGV;

   debug_printf2("Server intercepted execlp on %s\n", path);
   result = translateArgv(path, argv, &modified_argv);
   if (result == -1)
      result = orig_execvp(path, argv);
   else
      result = orig_execvp(modified_argv[0], modified_argv);
   error = errno;

   VARARG_TO_ARGV_CLEANUP;
   cleanupArgv(modified_argv);
   errno = error;
   
   return result;
}

static int execvp_wrapper(const char *path, char *const argv[])
{
   int error, result;
   char **modified_argv;

   debug_printf2("Server intercepted execvp on %s\n", path);
   result = translateArgv(path, (char **) argv, &modified_argv);
   if (result == -1)
      result = orig_execvp(path, argv);
   else
      result = orig_execvp(modified_argv[0], modified_argv);
   error = errno;
   cleanupArgv(modified_argv);
   errno = error;
   return result;
}
