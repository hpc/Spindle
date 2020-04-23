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

#include <mpi.h>
#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <unistd.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "spindle.h"
#include "ccwarns.h"

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
   om_ldpreload,
   om_dependency,
   om_dlopen,
   om_dlreopen,
   om_reorder,
   om_partial,
   om_spindleapi
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
#define FLAGS_SYMLINK  (1 << 2)
#define FLAGS_SKIP     (1 << 3)
int abort_mode = 0;
int fork_mode = 0;
int fork_child = 0;
int forkexec_mode = 0;
int nompi_mode = 0;
int preload_mode = 0;
int chdir_mode = 0;

int gargc;
char **gargv;

static int getUniqueHostPerNode();
static int collectResults();
static int collect_forkmode(int passed);
static void setup_forkmode();

GCC7_DISABLE_WARNING("-Wformat-truncation");

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
   { "libsymlink.so", NULL, NULL, UNLOADED, FLAGS_SYMLINK | FLAGS_SKIP },
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
extern void setup_func_callback(cb_func_t);

static char oldcwd[4096];

char *libpath(char *s) {
   static char path[4096];
   if (chdir_mode)
      snprintf(path, 4096, "%s/%s", oldcwd, s);
   else
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
   if (libraries[i].flags & FLAGS_SKIP) {
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
   for (i = 0; i<num_libraries; i++) {
      if (libraries[i].flags & FLAGS_NOEXIST || libraries[i].flags & FLAGS_SKIP)
         continue;
      libraries[i].opened = STARTUP_LOAD;
      test_printf("dlstart %s\n", libraries[i].libname);
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
      if (libraries[i].flags & FLAGS_NOEXIST || libraries[i].flags & FLAGS_SKIP)
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

void partial_mode()
{
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

void api_mode()
{
   int i;
   errno = 0;
   for (i = 0; libraries[i].libname; i++) {
      int result;
      struct stat buf1, buf2;
      char *path = libpath(libraries[i].libname);
      
      result = spindle_stat(path, &buf1);
      if (libraries[i].flags & FLAGS_NOEXIST) {
         if (result != -1 || errno != ENOENT) {
            err_printf("Bad error return from spindle_stat\n");
         }
         memset(&buf1, 0, sizeof(buf1));
         result = 0;
      }
      if (result == -1) {
         err_printf("Failed to spindle_stat file %s\n", path);
      }

      result = stat(path, &buf2);
      if (libraries[i].flags & FLAGS_NOEXIST) {
         if (result != -1 || errno != ENOENT) {
            err_printf("Bad error return from stat\n");
         }
         memset(&buf2, 0, sizeof(buf2));
         result = 0;
      }
      if (result == -1) {
         err_printf("Failed to stat file %s\n", path);
      }

      if (buf1.st_size != buf2.st_size) {
         err_printf("Failed, stats gave different sizes on %s\n", path);
      }

      if (libraries[i].flags & FLAGS_SYMLINK) {
         struct stat lbuf1, lbuf2;
         result = spindle_lstat(path, &lbuf1);
         if (result == -1) {
            err_printf("Failed to spindle_lstat file %s\n", path);
         }
         result = lstat(path, &lbuf2);
         if (result == -1) {
            err_printf("Failed to lstat file %s\n", path);
         }
         if (lbuf1.st_size != lbuf2.st_size) {
            err_printf("Failed, lstats gave different sizes on %s\n", path);
         }
         if (!S_ISLNK(lbuf1.st_mode)) {
            err_printf("Failed, spindle_lstat wasn't to symbolic link\n");
         }
         if (!S_ISLNK(lbuf2.st_mode)) {
            err_printf("Failed, lstat wasn't to symbolic link\n");
         }
      }

      uint32_t sig = 0;

      if (i % 2 == 0) {
         int fd = spindle_open(path, O_RDONLY);
         if (libraries[i].flags & FLAGS_NOEXIST) {
            if (fd != -1 || errno != ENOENT) {
               err_printf("Bad error return from spindle_stat\n");
            }
            continue;
         }
         if (fd == -1) {
            err_printf("Failed to open %s\n", path);
         }
         else {
            int result = read(fd, &sig, sizeof(sig));
            if (result == -1) {
               err_printf("Failed to read header from %s\n", path);
            }
            close(fd);
         }
      }
      else {
         FILE *fd = spindle_fopen(path, "r");
         if (libraries[i].flags & FLAGS_NOEXIST) {
            if (fd != NULL || errno != ENOENT) {
               err_printf("Bad error return from spindle_stat\n");
            }
            continue;
         }
         if (fd == NULL) {
            err_printf("Failed to fopen %s\n", path);
         }
         else {
            int result = fread(&sig, sizeof(sig), 1, fd);
            if (result != 1) {
               err_printf("Failed to fread from %s\n", path);
            }
            fclose(fd);
         }
      }

      if (sig != 0x7F454C46 && sig != 0x464c457f) {
         err_printf("Read file header %x, which wasn't elf header from %s\n", sig, path);
      }
   }
}

void open_libraries()
{
   switch (open_mode) {
      case om_unset:
         err_printf("Open mode was not set\n");
         break;
      case om_ldpreload:
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
      case om_spindleapi:
         api_mode();
         break;
   }      
}

static int run_exec_test(const char *prefix, const char *path, int expected)
{
   int pathsearch;
   char newpath[4097];
   newpath[4096] = '\0';
   if (prefix) {
      snprintf(newpath, 4097, "%s/%s", prefix, path);
      pathsearch = 0;
   }
   else {
      snprintf(newpath, 4097, "%s", path);
      pathsearch = 1;
   }

   int pid = fork();
   if (pid == -1) {
      err_printf("%s could not fork\n", newpath);
      return -1;
   }
   if (pid == 0) {
      if (expected == 0) {
         close(-1); /* A close call does a check_for_fork under the hood, re-initing the
                       logger connection for the following print */
         test_printf("dlstart %s\n", path);
      }
      
      char* args[2];
      args[0] = newpath;
      args[1] = NULL;
      if (pathsearch)
         execvp(newpath, args);
      else
         execv(newpath, args);
      exit(errno);
   }

   int status, result;
   do {
      result = waitpid(pid, &status, 0);
      if (WIFSIGNALED(status)) {
         err_printf("%s unexpectedly exited on signal %d\n", newpath, WTERMSIG(status));
         return -1;
      }
      if (result == -1) {
         err_printf("%s had unexpected waitpid failure\n", newpath);
         return -1;
      }
   } while (result != pid && !WIFEXITED(status));
   if (expected != WEXITSTATUS(status)) {
      err_printf("%s exited with return code %d, expected %d\n", newpath, WEXITSTATUS(status), expected);
      return -1;
   }
   return 0;
}

static int run_exec_sets(const char *prefix)
{
   int result = 0;

   result |= run_exec_test(prefix, "retzero_rx", 0);
   result |= run_exec_test(prefix, "retzero_x", 0);
   result |= run_exec_test(prefix, "retzero_r", EACCES);
   result |= run_exec_test(prefix, "retzero_", EACCES);   
   result |= run_exec_test(prefix, "nofile", ENOENT);
   result |= run_exec_test(prefix, "nodir/nofile", ENOENT);
   result |= run_exec_test(prefix, "..", EACCES);
   //result |= run_exec_test(prefix, "badinterp", ENOENT);
   return result;
}

static int run_execs()
{
   if (fork_mode || forkexec_mode || nompi_mode || chdir_mode)
      return 0;
   
   int result = run_exec_sets(STR(LPATH));
   if (result == -1)
      return -1;
      
   //Add LPATH to PATH environment variable and run again without a prefix
   char *path = getenv("PATH");
   int len = strlen(path) + strlen(STR(LPATH)) + 2;
   char *newpath = (char *) malloc(len);
   snprintf(newpath, len, "%s:%s", path, STR(LPATH));
   return run_exec_sets(NULL);
}


#define STAT 1
#define LSTAT 2
#define FSTAT 4
static dev_t device;
static int run_stat_test(const char *file, int flags, mode_t prot, int expected)
{
   struct stat buf;
   int result = -1;
   int fd = -1;
   const char *statname = NULL;

   if (expected == 0)
      test_printf("dlstart %s\n", file);
   if (flags & LSTAT) {
      statname = "lstat";
      result = lstat(file, &buf);
   }
   else if (flags & FSTAT) {
      statname = "fstat";      
      fd = open(file, O_RDONLY);
      if (fd != -1)
         result = fstat(fd, &buf);
   }
   else if (flags & STAT) {
      statname = "stat";      
      result = stat(file, &buf);
   }
   if (result == -1)
      result = errno;
   if (fd != -1)
      close(fd);
   
   if (result != expected) {
      err_printf("Expected return value %d, got return value %d from %s test of %s\n",
                 expected, result, statname, file);
      return -1;
   }
   if (result)
      //Expected error return, do not test buf
      return 0;
   
   if (buf.st_dev != device) {
      err_printf("Expected device %d, got device %d on %s test of %s\n",
                 (int) device, (int) buf.st_dev, statname, file);
      return -1;
   }
   if (prot && ((buf.st_mode & 0700) != prot)) {
      err_printf("Expected prot %o, got prot %o on %s test of %s\n",
                 prot, buf.st_mode & 0700, statname, file);
      return -1;      
   }

   return 0;
}

static int set_device()
{
   struct stat buf;
   int result;
   
   result = stat("retzero_rx", &buf);
   if (result == -1) {
      err_printf("Could not get device of retzero_rx");
      return -1;
   }
   device = buf.st_dev;
   return 0;
}

static int run_stats()
{
   int result;

   if (fork_mode || forkexec_mode || nompi_mode || chdir_mode)
      return 0;
   
   result = set_device();

   result |= run_stat_test("hello_r.py", STAT, 0600, 0);
   result |= run_stat_test("hello_x.py", STAT, 0300, 0);
   result |= run_stat_test("hello_rx.py", STAT, 0700, 0);
   result |= run_stat_test("hello_.py", STAT, 0200, 0);
   result |= run_stat_test("hello_l.py", STAT, 0300, 0);
   result |= run_stat_test("noexist.py", STAT, 0000, ENOENT);
   result |= run_stat_test("/nodir/nofile.py", STAT, 0000, ENOENT);
   /* result |= run_stat_test("retzero_/nofile.py", STAT, 0000, ENOTDIR); */
   result |= run_stat_test("badlink.py", STAT, 0000, ENOENT);
   result |= run_stat_test(".", STAT, 0000, 0);
   result |= run_stat_test(NULL, STAT, 0000, EFAULT);

   result |= run_stat_test("hello_r.py", FSTAT, 0600, 0);
   result |= run_stat_test("hello_x.py", FSTAT, 0000, EACCES);
   result |= run_stat_test("hello_rx.py", FSTAT, 0700, 0);
   result |= run_stat_test("hello_.py", FSTAT, 0000, EACCES);
   result |= run_stat_test("hello_l.py", FSTAT, 0000, EACCES);
   result |= run_stat_test("noexist.py", FSTAT, 0000, ENOENT);
   result |= run_stat_test("/nodir/nofile.py", FSTAT, 0000, ENOENT);
   /* result |= run_stat_test("retzero_/nofile.py", FSTAT, 0000, ENOTDIR); */
   result |= run_stat_test("badlink.py", FSTAT, 0000, ENOENT);
   result |= run_stat_test(".", FSTAT, 0000, 0);
   result |= run_stat_test(NULL, FSTAT, 0000, EFAULT);

   result |= run_stat_test("hello_r.py", LSTAT, 0600, 0);
   result |= run_stat_test("hello_x.py", LSTAT, 0300, 0);
   result |= run_stat_test("hello_rx.py", LSTAT, 0700, 0);
   result |= run_stat_test("hello_.py", LSTAT, 0200, 0);
   result |= run_stat_test("hello_l.py", LSTAT, 0700, 0);
   result |= run_stat_test("noexist.py", LSTAT, 0000, ENOENT);
   result |= run_stat_test("/nodir/nofile.py", LSTAT, 0000, ENOENT);
   /* result |= run_stat_test("retzero_/nofile.py", LSTAT, 0000, ENOTDIR); */
   result |= run_stat_test("badlink.py", LSTAT, 0700, 0);
   result |= run_stat_test(".", LSTAT, 0000, 0);
   result |= run_stat_test(NULL, LSTAT, 0000, EFAULT);
   
   return result;
}


void push_cwd()
{
   getcwd(oldcwd, 4096);
   chdir("..");
}

void pop_cwd()
{
   chdir(oldcwd);
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
      TEST_ARG(ldpreload);
      TEST_ARG(dependency);
      TEST_ARG(dlopen);
      TEST_ARG(dlreopen);
      TEST_ARG(reorder);
      TEST_ARG(partial);
      TEST_ARG(spindleapi);
      MODE_ARG(abort);
      MODE_ARG(fork);
      MODE_ARG(forkexec);
      MODE_ARG(nompi);
      MODE_ARG(preload);
      MODE_ARG(chdir);
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
   if (open_mode == om_spindleapi)
      return;

   for (i=0; i<num_libraries; i++) {
      if ((open_mode != om_partial || (libraries[i].flags & FLAGS_MUSTOPEN)) && 
          (!(libraries[i].flags & FLAGS_NOEXIST)) && 
          (!(libraries[i].flags & FLAGS_SKIP)) &&
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

   if (chdir_mode)
      push_cwd();

   open_libraries();
   if (had_error)
      return -1;

   run_execs();
   if (had_error)
      return -1;

   run_stats();
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

   if (chdir_mode)
      pop_cwd();
   
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
         err_printf("Forked child %d exited with signal %d\n", fork_child, WTERMSIG(status));
         return -1;
      }
      else if (WIFEXITED(status)) {
         if (WEXITSTATUS(status) == 0) {
            return 0;
         }
         else {
            err_printf("Forked child %d exited with status %d\n", fork_child, WEXITSTATUS(status));
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
   
GCC7_ENABLE_WARNING
