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

#include <mpi.h>
#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <unistd.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#if !defined(LPATH)
#error LPATH must be defined
#endif

#define STR(x) STR2(x)
#define STR2(x) #x

#if defined(__cplusplus)
extern "C" {
#endif
   extern void spindle_test_log_msg(char *s);
#if defined(__cplusplus)
}
#endif

static char test_buffer[4096];
#define MAX_STR_SIZE 4096

int had_error;
#define err_printf(format, ...)                                         \
   do {                                                                 \
      snprintf(test_buffer, 4096, "Error - [%s:%u] - " format, __FILE__, __LINE__, ## __VA_ARGS__); \
      spindle_test_log_msg(test_buffer);                                \
      had_error = 1;                                                    \
   } while (0); 

#define test_printf(format, ...)                                        \
   do {                                                                 \
      snprintf(test_buffer, 4096, format, ## __VA_ARGS__);              \
      spindle_test_log_msg(test_buffer);                                \
   } while(0);                                                          \

typedef enum 
{
   om_unset,
   om_preload,
   om_dependency,
   om_dlopen,
   om_dlreopen,
   om_reorder,
   om_partial
} open_mode_t;
open_mode_t open_mode = om_unset;

typedef struct {
   char *libname;
   void *dlhandle;
   int (*calc_func)(void);
   int opened;
   int flags;
} open_libraries_t;

#define UNLOADED 0
#define DLOPENED 1
#define STARTUP_LOAD 2

#define FLAGS_MUSTOPEN (1 << 0)
#define FLAGS_NOEXIST  (1 << 1)

int abort_mode = 0;
int fork_mode = 0;
int fork_child = 0;
int forkexec_mode = 0;
int nompi_mode = 0;

int gargc;
char **gargv;

static int getUniqueHostPerNode();
static int collectResults();
static int collect_forkmode(int passed);
static void setup_forkmode();

open_libraries_t libraries[] = {
   { "libtest10.so", NULL, NULL, UNLOADED, FLAGS_MUSTOPEN },
   { "libtest50.so", NULL, NULL, UNLOADED, 0 },
   { "libtest100.so", NULL, NULL, UNLOADED, 0 },
   { "libtest500.so", NULL, NULL, UNLOADED, 0 },
   { "libtest1000.so", NULL, NULL, UNLOADED, 0 },
   { "libtest2000.so", NULL, NULL, UNLOADED, 0 },
   { "libtest4000.so", NULL, NULL, UNLOADED, 0 },
   { "libtest6000.so", NULL, NULL, UNLOADED, 0 },
   { "libtest8000.so", NULL, NULL, UNLOADED, 0 },
   { "libtest10000.so", NULL, NULL, UNLOADED, 0 },
   { "libdepA.so", NULL, NULL, UNLOADED, 0 },
   { "libcxxexceptA.so", NULL, NULL, UNLOADED, 0 },
   { "libnoexist.so", NULL, NULL, UNLOADED, FLAGS_NOEXIST },
   { NULL, NULL, NULL, 0, 0 }
};
int num_libraries;

#define DEPENDENCY_HANDLE ((void *) 1)

void get_calc_function(int (*func)(void), char *name)
{
   int i;
   for (i = 0; libraries[i].libname; i++) {
      if (strcmp(libraries[i].libname, name) != 0)
         continue;
      libraries[i].calc_func = func;
      return;
   }
   err_printf("Failed to find function %s in list\n", name);
}

typedef int (*func_t)(void);
typedef void (*cb_func_t)(func_t, char *);
extern setup_func_callback(cb_func_t);

char *libpath(char *s) {
   static char path[4096];
   snprintf(path, 4096, "%s/%s", STR(LPATH), s);
   return path;
}

static void open_library(int i)
{
   char *fullpath, *result;
   test_printf("dlstart %s\n", libraries[i].libname);
   fullpath = libpath(libraries[i].libname);
   result = dlopen(fullpath, RTLD_LAZY | RTLD_GLOBAL);
   if (libraries[i].flags & FLAGS_NOEXIST) {
      if (result != 0)
         err_printf("Failure, opened a library that doesn't exist\n");
      return;
   }
   libraries[i].opened = DLOPENED;
   if (!result) {
      err_printf("Failed to dlopen library %s: %s\n", fullpath, dlerror());
      return;
   }
   libraries[i].dlhandle = result;
}

static int hostname_seed()
{
   int seed = 0, i;
   char hostname[1024];
   gethostname(hostname, 1024);
   for (i=0; hostname[i] != '\0'; i++) {
      seed += hostname[i];
   }
   return seed;
}

void dependency_mode()
{
   /* Should be auto loaded */
   int i;
   test_printf("dlstart %s\n", libraries[i].libname);
   for (i = 0; i<num_libraries; i++) {
      if (libraries[i].flags & FLAGS_NOEXIST)
         continue;
      libraries[i].opened = STARTUP_LOAD;
   }
}

void ldpreload_mode()
{
   unsigned i;
   char *env = getenv("LD_PRELOAD");
   if (!env) {
      err_printf("LD_PRELOAD Unset\n");
      return;
   }

   dependency_mode();
   for (i = 0; i < num_libraries; i++) {
      if (libraries[i].flags & FLAGS_NOEXIST)
         continue;
      if (strstr(env, libraries[i].libname) == NULL) {
         err_printf("Could not find library %s in LD_PRELOAD (%s)\n", libraries[i].libname, env);
         return;
      }
   }
}

void dlopen_mode()
{
   int i;
   for (i = 0; i<num_libraries; i++) {
      open_library(i);
   }
}

void dlreopen_mode()
{
   dependency_mode();
   dlopen_mode();
}

void reorder_mode()
{
   /* Load libraries in random (based on hostname) order */
   int i, initial;
   i = initial = hostname_seed() % num_libraries;
   do {
      open_library(i);
      i++;
      if (i == num_libraries) {
         i = 0;
      }
   } while (i != initial);
}

void partial_mode() {
   /* Load some libraries in random (based on hostname) order,
      randomly skip approximately half the libraries. */
   int i, initial;
   int seed = hostname_seed();
   srand(seed);
   i = initial = (seed % num_libraries);
   do {
      if ((rand() & 1) || (libraries[i].flags & FLAGS_MUSTOPEN)) {
         open_library(i);
      }
      i++;
      if (i == num_libraries) {
         i = 0;
      }
   } while (i != initial);   
}

void open_libraries()
{
   switch (open_mode) {
      case om_unset:
         err_printf("Open mode was not set\n");
         break;
      case om_preload:
         ldpreload_mode();
         break;
      case om_dependency:
         dependency_mode();
         break;
      case om_dlopen:
         dlopen_mode();
         break;
      case om_dlreopen:
         dlreopen_mode();
         break;
      case om_reorder:
         reorder_mode();
         break;
      case om_partial:
         partial_mode();
         break;
   }      
}

#define TEST_ARG(X) if (strcmp(argv[i], "--" STR(X)) == 0) open_mode = om_ ## X
#define MODE_ARG(X) if (strcmp(argv[i], "--" STR(X)) == 0) X ## _mode = 1;
void parse_args(int argc, char *argv[])
{
   if (strstr(argv[0], "test_driver") == NULL) {
      err_printf("Did not find test_driver on command line\n");
   }
   int i;
   for (i = 0; i < argc; i++) {
      TEST_ARG(preload);
      TEST_ARG(dependency);
      TEST_ARG(dlopen);
      TEST_ARG(dlreopen);
      TEST_ARG(reorder);
      TEST_ARG(partial);
      MODE_ARG(abort);
      MODE_ARG(fork);
      MODE_ARG(forkexec);
      MODE_ARG(nompi);
   }
   gargc = argc;
   gargv = argv;
   
   if (forkexec_mode)
      fork_mode = 1;
}

void call_funcs()
{
   int i, result;
   for (i = 0; i<num_libraries; i++) {
      if (libraries[i].opened == UNLOADED)
         continue;
      result = libraries[i].calc_func();
      if (result == 0) {
         err_printf("Unexpected return result of 0 from library %s\n", libraries[i].libname);
      }
   }
}

void check_libraries()
{
   int i;
   for (i=0; i<num_libraries; i++) {
      if ((open_mode != om_partial || (libraries[i].flags & FLAGS_MUSTOPEN)) && 
          (!(libraries[i].flags & FLAGS_NOEXIST)) && 
          (!libraries[i].calc_func)) {
         err_printf("Didn't open expected library %s\n", libraries[i].libname); 
      }
      if (libraries[i].opened == UNLOADED)
         continue;
      if (libraries[i].opened == DLOPENED && !libraries[i].dlhandle)
         err_printf("Failed to dlopen library %s\n", libraries[i].libname);
      if (!libraries[i].calc_func) {
         err_printf("Failed to run library constructor in %s\n", libraries[i].libname);
      }
   }
}

void close_libs()
{
   int i, result;
   for (i=0; i<num_libraries; i++) {
      if (!libraries[i].dlhandle)
         continue;
      result = dlclose(libraries[i].dlhandle);
      if (result != 0)
         err_printf("Failed to close library %s: %s\n", libraries[i].libname, dlerror());
   }
}

int run_test()
{
   /*Make get_calc_function get called when a library loads.
     if a library has already been loaded, get_calc_function
     will be called now. */
   setup_func_callback(get_calc_function);

   open_libraries();
   if (had_error)
      return -1;

   check_libraries();
   if (had_error)
      return -1;

   call_funcs();
   if (had_error)
      return -1;

   close_libs();
   if (had_error)
      return -1;
   
   return 0;
}

int hash_start_val = 5381;
int hash(char *str)
{
   int hash = hash_start_val;
   int c;
   
   while ((c = *str++))
      hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
   
   hash_start_val += 7;
   return hash;
}

int main(int argc, char *argv[])
{
   int result = 0;
   int rank = -1;
   int passed;

   if (getenv("SPINDLE_TEST") == NULL) {
      fprintf(stderr, "Enable environment variable SPINDLE_TEST before running!");
      return -1;
   }

   open_libraries_t *cur_lib;
   for (cur_lib = libraries; cur_lib->libname; cur_lib++, num_libraries++);

   parse_args(argc, argv);

   if (!nompi_mode) {
      result = MPI_Init(&argc, &argv);
      MPI_Comm_rank(MPI_COMM_WORLD, &rank);
   }
   
   /* Setup */
   if (fork_mode)
      setup_forkmode();

   /* Run test */
   result = run_test();

   /* Check results */
   passed = (result == 0);
   if (fork_mode && collect_forkmode(passed) == -1)
      passed = 0;
   if (!nompi_mode && !collectResults())
      passed = 0;

   if (!nompi_mode)
      MPI_Finalize();

   if (rank == 0) {
      if (passed)
         printf("PASSED.\n");
      else
         printf("FAILED.\n");
   }

   if (abort_mode) {
      if (nompi_mode)
         abort();
      else
         MPI_Abort(MPI_COMM_WORLD, 0);
   }

   return passed ? 0 : -1;
}

static void setup_forkmode() {
   fork_child = fork();
   if (fork_child == -1) {
      err_printf("Unable to fork child process: %s\n", strerror(errno));
      return;
   }

   if (fork_child == 0 && forkexec_mode) {
      int i;
      char **newargv = (char **) malloc(sizeof(char *) * (gargc+1));
      for (i = 0; i < gargc; i++) {
         if (strstr(gargv[i], "fork")) {
            newargv[i] = "--nompi";
         }
         else
            newargv[i] = gargv[i];
      }
      newargv[gargc] = NULL;
      execv(newargv[0], newargv);
      err_printf("Failed to exec: %s\n", newargv[0]);
      exit(-1);
   }
}

static int collect_forkmode(int passed) {
   if (!fork_child) {
      exit(passed ? 0 : -1);
   }
   else {
      int status, result;
      do {
         result = waitpid(fork_child, &status, 0);
      } while (result == -1 && errno == EINTR);
      if (WIFSIGNALED(status)) {
         err_printf("Forked child exited with signal %d\n", WTERMSIG(status));
         return -1;
      }
      else if (WIFEXITED(status)) {
         if (WEXITSTATUS(status) == 0) {
            return 0;
         }
         else {
            err_printf("Forked chidl exited with status %d\n", WEXITSTATUS(status));
            return -1;
         }
      }
      else {
         assert(0);
      }
   }
   return -1;
}

/**
 * One process from each node in an MPI job will return true,
 * others will return false.
 **/
static int getUniqueHostPerNode()
{
   int color, global_rank;
   int size, rank;
   int set_oldcomm = 0;
   MPI_Comm newcomm, oldcomm = MPI_COMM_WORLD;

   MPI_Comm_rank(MPI_COMM_WORLD, &global_rank);

   for (;;) {
      char name[MAX_STR_SIZE], oname[MAX_STR_SIZE];
      memset(name, 0, MAX_STR_SIZE);
      gethostname(name, MAX_STR_SIZE);
      color = hash(name);
      if (color < 0)
         color *= -1;
      
      int result = MPI_Comm_split(oldcomm, color, global_rank, &newcomm);
      if (result != MPI_SUCCESS) {
         fprintf(stderr, "Error in MPI_Comm_split\n");
         MPI_Abort(MPI_COMM_WORLD, -1);
      }
      
      if (set_oldcomm) {
         MPI_Comm_free(&oldcomm);
      }

      MPI_Comm_rank(newcomm, &rank);
      MPI_Comm_size(newcomm, &size);
      
      if (rank == 0)
         memcpy(oname, name, MAX_STR_SIZE);
      result = MPI_Bcast(oname, MAX_STR_SIZE, MPI_CHAR, 0, newcomm);
      if (result != MPI_SUCCESS) {
         fprintf(stderr, "Error in MPI_Scatter\n");
         MPI_Abort(MPI_COMM_WORLD, -1);
      }

      int global_str_match = 0;
      int str_match = (strcmp(name, oname) == 0);
      result = MPI_Allreduce(&str_match, &global_str_match, 1, MPI_INT, MPI_LAND, newcomm);
      if (result != MPI_SUCCESS) {
         fprintf(stderr, "Error in MPI_Allreduce\n");
         MPI_Abort(MPI_COMM_WORLD, -1);
      }

      if (global_str_match) {
         break;
      }
      
      set_oldcomm = 1;
      oldcomm = newcomm;
   }
   
   int result = MPI_Barrier(MPI_COMM_WORLD);
   if (result != MPI_SUCCESS) {
      fprintf(stderr, "Error in MPI_Barrier\n");
      MPI_Abort(MPI_COMM_WORLD, -1);
   }

   return (rank == 0);
}

static int collectResults()
{
   int is_unique = getUniqueHostPerNode();
   int test_passed = 1;
   char filename_passed[MAX_STR_SIZE];
   char filename_failed[MAX_STR_SIZE];

   if (is_unique) {
      char *tempdir = getenv("TMPDIR");
      if (!tempdir)
         tempdir = getenv("TEMPDIR");
      if (!tempdir)
         tempdir = "/tmp";

      snprintf(filename_passed, MAX_STR_SIZE, "%s/spindle_test_passed", tempdir);
      filename_passed[MAX_STR_SIZE-1] = '\0';
      snprintf(filename_failed, MAX_STR_SIZE, "%s/spindle_test_failed", tempdir);
      filename_failed[MAX_STR_SIZE-1] = '\0';

      unlink(filename_passed);
      unlink(filename_failed);
      test_printf("done\n");
      int timeout = 100; //10 seconds
      for (;;) {
         struct stat buf;
         if (stat(filename_passed, &buf) != -1) {
            test_passed = 1;
            break;
         }
         if (stat(filename_failed, &buf) != -1) {
            test_passed = 0;
            break;
         }
         if (timeout-- == 0) {
            fprintf(stderr, "[%s:%u] - Timeout waiting for test result\n", __FILE__, __LINE__);
            test_passed = 0;
            break;
         }
         usleep(100000);
      }
      unlink(filename_passed);
      unlink(filename_failed);
   }

   int global_test_passed = 0;
   int result = MPI_Allreduce(&test_passed, &global_test_passed, 1, MPI_INT, MPI_LAND, MPI_COMM_WORLD);
   if (result != MPI_SUCCESS) {
      fprintf(stderr, "Error in MPI_Allreduce #2\n");
      MPI_Abort(MPI_COMM_WORLD, -1);
   }
   
   return global_test_passed;
}
   
