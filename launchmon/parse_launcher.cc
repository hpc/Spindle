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

#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <assert.h>
#include <stdio.h>

#include "parse_launcher.h"

#define FL_LAUNCHER       1<<0
#define FL_GNU_PARAM      1<<2
#define FL_OPTIONAL       1<<3
#define FL_INTEGER        1<<4
#define FL_CHDIR          1<<5

typedef struct {
   const char *opt;
   const char *long_opt;
   int flags;
} cmdoption_t;

#define srun_size (sizeof(srun_options) / sizeof(cmdoption_t))
static cmdoption_t srun_options[] = {
   { "srun", NULL,                   FL_LAUNCHER },
   { "-A",   "--account",            FL_GNU_PARAM },
   { NULL,   "--begin",              FL_GNU_PARAM },
   { "-c",   "--cpus-per-task",      FL_GNU_PARAM },
   { NULL,   "--checkpoint",         FL_GNU_PARAM },
   { NULL,   "--checkpoint-dir",     FL_GNU_PARAM },
   { NULL,   "--comment",            FL_GNU_PARAM },
   { "-d",   "--dependency",         FL_GNU_PARAM },
   { "-D",   "--chdir",              FL_GNU_PARAM | FL_CHDIR },
   { "-e",   "--error",              FL_GNU_PARAM },
   { NULL,   "--epilog",             FL_GNU_PARAM },
   { "-E",   "--preserve-env",       0 },
   { NULL,   "--get-user-env",       0 },
   { NULL,   "--gres",               FL_GNU_PARAM },
   { "-H",   "--hold",               0 },
   { "-i",   "--input",              FL_GNU_PARAM },
   { "-I",   "--immediate",          FL_OPTIONAL | FL_GNU_PARAM | FL_INTEGER },
   { NULL,   "--jobid",              FL_GNU_PARAM },
   { "-J",   "--job-name",           FL_GNU_PARAM },
   { "-k",   "--no-kill",            0 },
   { "-K",   "--kill-on-bad-exit",   0 },
   { "-l",   "--label",              0 },
   { "-L",   "--licenses",           FL_GNU_PARAM },
   { "-m",   "--distribution",       FL_GNU_PARAM },
   { NULL,   "--mail-type",          FL_GNU_PARAM },
   { NULL,   "--mail-user",          FL_GNU_PARAM },
   { NULL,   "--mpi",                FL_GNU_PARAM },
   { NULL,   "--multi-prog",         0 },
   { "-n",   "--ntasks",             FL_GNU_PARAM },
   { NULL,   "--nice",               FL_OPTIONAL | FL_GNU_PARAM | FL_INTEGER },
   { NULL,   "--ntasks-per-node",    FL_GNU_PARAM },
   { "-N",   "--nodes",              FL_GNU_PARAM },
   { "-o",   "--output",             FL_GNU_PARAM },
   { "-O",   "--overcommit",         0 },
   { "-p",   "--partition",          FL_GNU_PARAM },
   { NULL,   "--prolog",             FL_GNU_PARAM },
   { NULL,   "--propagate",          FL_OPTIONAL | FL_GNU_PARAM },
   { NULL,   "--pty",                0 },
   { "-q",   "--quit-on-interrupt",  0 },
   { NULL,   "--qos",                FL_GNU_PARAM },
   { "-Q",   "--quiet",              0 },
   { "-r",   "--relative",           FL_GNU_PARAM },
   { NULL,   "--restart-dir",        FL_GNU_PARAM },
   { "-s",   "--share",              0 },
   { NULL,   "--slurmd-debug",       FL_GNU_PARAM },
   { NULL,   "--task-epilog",        FL_GNU_PARAM },
   { NULL,   "--task-prolog",        FL_GNU_PARAM },
   { "-T",   "--threads",            FL_GNU_PARAM },
   { "-t",   "--time",               FL_GNU_PARAM },
   { NULL,   "--time-min",           FL_GNU_PARAM },
   { "-u",   "--unbuffered",         0 },
   { "-v",   "--verbose",            0 },
   { "-W",   "--wait",               FL_GNU_PARAM },
   { "-X",   "--disable-status",     0 },
   { NULL,   "--switch",             FL_GNU_PARAM },
   { NULL,   "--contiguous",         0 },
   { "-C",   "--constraint",         FL_GNU_PARAM },
   { NULL,   "--mem",                FL_GNU_PARAM },
   { NULL,   "--mincpus",            FL_GNU_PARAM },
   { NULL,   "--reservation",        FL_GNU_PARAM },
   { NULL,   "--tmp",                FL_GNU_PARAM },
   { "-w",   "--nodelist",           FL_GNU_PARAM },
   { "-x",   "--exclude",            FL_GNU_PARAM },
   { "-Z",   "--no-allocate",        0 },
   { NULL,   "--exclusive",          0 },
   { NULL,   "--mem-per-cpu",        FL_GNU_PARAM },
   { NULL,   "--resv-ports",         0 },
   { "-B",   "--extra-node-info",    FL_GNU_PARAM },
   { NULL,   "--sockets-per-node",   FL_GNU_PARAM },
   { NULL,   "--cores-per-socket",   FL_GNU_PARAM },
   { NULL,   "--threads-per-core",   FL_GNU_PARAM },
   { NULL,   "--ntasks-per-core",    FL_GNU_PARAM },
   { NULL,   "--ntasks-per-socket",  FL_GNU_PARAM },
   { NULL,   "--use-env",            FL_GNU_PARAM },
   { NULL,   "--auto-affinity",      FL_GNU_PARAM },
   { NULL,   "--io-watchdog",        FL_GNU_PARAM },
   { NULL,   "--renice",             FL_GNU_PARAM },
   { NULL,   "--thp",                FL_GNU_PARAM },
   { NULL,   "--overcommit-memory",  FL_GNU_PARAM },
   { NULL,   "--overcommit-ratio",   FL_GNU_PARAM },
   { NULL,   "--private-namespace",  0 },
   { NULL,   "--hugepages",          FL_GNU_PARAM },
   { NULL,   "--drop-caches",        FL_GNU_PARAM },
   { "-h",   "--help",               0 },
   { NULL,   "--usage",              0 },
   { "-V",   "--version",            0 },
};

static int isIntegerString(char *s)
{
   if (*s >= '0' && *s <= '9')
      return 1;
   if ((*s == '-' || *s == '+') && 
       s[1] >= '0' && s[1] <= '9')
      return 1;
   return 0;
}

static int isExec(char *s, char *dir_s)
{
   int result;
   struct stat buf;
   
   if (dir_s) {
      if (*s == '/') {
         /* Absolute path, ignore dir_s */
         return isExec(s, NULL);
      }
      int s_strlen = strlen(s);
      int dir_strlen = strlen(dir_s);
      char *newstr = (char *) malloc(dir_strlen + 1 + s_strlen + 1);
      strcpy(newstr, dir_s);
      if (dir_s[dir_strlen-1] != '/') {
         strcat(newstr, "/");
      }
      strcat(newstr, s);
      result = isExec(newstr, NULL);
      free(newstr);
      return result;
   }
   
   result = stat(s, &buf);
   if (result == -1)
      return 0;

   if ((buf.st_mode & S_IFLNK) == S_IFLNK) {
      /* Dereference any symbolic link and try again */
      char *realname = realpath(s, NULL);
      result = isExec(realname, NULL);
      free(realname);
      return result;
   }

   /* Return true if not a directory and at least one execute bit is set */
   return (!(buf.st_mode & S_IFDIR) && (buf.st_mode & 0111));
}

static int isPathExec(char *s)
{
   char *path, *r;
   if (*s == '/') {
      /* Absolute file path.  Not searched in PATH. */
      return 0;
   }
   path = getenv("PATH");
   if (!path) {
      return 0;
   }
   path = strdup(path);

   for (r = strtok(path, ":"); r != NULL; r = strtok(NULL, ":")) {
      if (isExec(s, r)) {
         free(path);
         return 1;
      }
   }
   free(path);
   return 0;
}

static int isExecutableFile(char *s, char *cwd_s)
{
   return isExec(s, cwd_s) || isPathExec(s);
}

static int parseLaunchCmdLine(int argc, char *argv[],
                              int *found_launcher_at, int *found_exec_at,
                              cmdoption_t *options, int options_len)
{
   int i, j;
   *found_launcher_at = -1;
   *found_exec_at = -1;
   char *chdir_s = NULL;

   /**
    * Find the location of the launcher (ie, srun or mpirun) in argv.  We're looking for
    * an exact match of 'launcher' or '/launcher' at the end of an argument
    **/
   //assert(options[0].flags == FL_LAUNCHER);
   for (i = 0; i < options_len; i++) {
      if (!(options[i].flags & FL_LAUNCHER))
         continue;
      const char *launcher_name = options[i].opt;
      assert(launcher_name);
      int launcher_name_size = strlen(launcher_name);
      assert(launcher_name_size);
      for (j = 0; j < argc; j++) {
         char *arg = argv[j];
         if (strcmp(arg, launcher_name) == 0) {
            *found_launcher_at = j;
            break;
         }
         int len = strlen(arg);
         if ((len >= launcher_name_size+1) &&
             (strcmp(arg + (len-launcher_name_size), launcher_name) == 0) &&
             (arg[len-launcher_name_size-1] == '/'))
         {
            *found_launcher_at = j;
            break;
         }
      }
      if (*found_launcher_at != -1)
         break;
   }
   if (*found_launcher_at == -1)
      return NO_LAUNCHER;


   /**
    * Parse launcher arguments, looking for executable
    **/
   for (i = (*found_launcher_at)+1; i < argc; i++) {
      char *arg = argv[i];
      int is_launcher_param = 0;

      for (j = 0; j < options_len; j++) {
         cmdoption_t *opt = options+j;
         if (opt->flags & FL_LAUNCHER) {
            continue;
         }
         else if (opt->flags & FL_GNU_PARAM) {
            /* GNU style options.  Short opts have a space and then a parameter,
               long opts have an = and a parameter */
            int long_opt_len = opt->long_opt ? strlen(opt->long_opt) : 0;
            int matched_long;
            if (opt->opt && strcmp(arg, opt->opt) == 0) {
               is_launcher_param = 1;
               matched_long = 0;
            }
            else if (opt->long_opt && strncmp(arg, opt->long_opt, long_opt_len) == 0) {
               is_launcher_param = 1;
               matched_long = 1;
            }
            else {
               continue;
            }

            if (opt->flags & FL_CHDIR) {
               /* This paramter will chdir before the executable will run.
                  Record its value so that we can find the exec with it. */
               if (matched_long) {
                  chdir_s = strchr(arg, '=');
                  if (chdir_s)
                     chdir_s++;
               }
               else {
                  chdir_s = (i + 1 < argc) ? argv[i+1] : NULL;
               }
               if (chdir_s && *chdir_s == '\0')
                  chdir_s = NULL;
            }

            /* Parse out the parameter */
            if (matched_long == 1) {
               /* Already consumed the parameters along with the long option */
            }
            else if (!(opt->flags & FL_OPTIONAL)) {
               /* Required parameter, must be in next slot */
               i++;
            }
            else if (i+1 < argc) {
               char *param = argv[i+1];
               if (*param == '-') {
                  /* Optional parameter not present, found a different option */
               }
               else if ((opt->flags & FL_INTEGER) && isIntegerString(param)) {
                  /* Optional integer parameter found, skipping */
                  i++;
               }
               else if (isExecutableFile(param, chdir_s)) {
                  /* Found exec candidate, not parameter */
               }
               else {
                  /* Found likely optional parameter.  Skipping past it. */
                  i++;
               }
            }
            break;
         }
         else if (opt->flags == 0) {
            /* Basic option.  Just match short or long opt */
            if ((opt->opt && strcmp(arg, opt->opt) == 0) ||
                (opt->long_opt && strcmp(arg, opt->long_opt) == 0))
            {
               is_launcher_param = 1;
               break;
            }
         }
         else {
            assert(0);
         }
      }

      if (is_launcher_param) {
         /* We've identified this as a launcher parameter. Move on. */
         continue;
      }
      if (*arg == '-') {
         /* This is an unrecognized command line argument.  Move on */
         continue;
      }
      if (isExecutableFile(arg, chdir_s)) {
         /* We found the executable.  Yeah */
         *found_exec_at = i;
         break;
      }

      /* If we get here, then it's likely an unrecognized launcher parameter */
   }

   if (*found_exec_at == -1)
      return NO_EXEC;

   return 0;
}

static int modifyCmdLineForLauncher(int argc, char *argv[],
                                    int *new_argc, char **new_argv[],
                                    cmdoption_t *options, int options_len,
                                    const char *ldcs_location,
                                    const char *ldcs_number,
                                    unsigned long ldcs_options,
                                    const char *bootstrapper_name)
{
   int result;
   int found_launcher_at, found_exec_at;
   int i, j;
   char ldcs_options_str[32];

   snprintf(ldcs_options_str, 32, "%lu", ldcs_options);
   /* Parse the command line */
   result = parseLaunchCmdLine(argc, argv, &found_launcher_at, &found_exec_at, options, options_len);
   if (result < 0)
      return result;

   /* Add the bootstrapper to the cmdline before the executable */
   *new_argc = argc + 4;
   *new_argv = (char **) malloc(sizeof(char *) * (*new_argc + 1));
   for (i = found_launcher_at, j = 0; i < argc; i++, j++) {
      if (i == found_exec_at) {
         (*new_argv)[j++] = strdup(bootstrapper_name);
         (*new_argv)[j++] = strdup(ldcs_location);
         (*new_argv)[j++] = strdup(ldcs_number);
         (*new_argv)[j++] = strdup(ldcs_options_str);
      }
      (*new_argv)[j] = argv[i];
   }
   (*new_argv)[j] = NULL;
   return 0;
}

int createNewCmdLine(int argc, char *argv[],
                     int *new_argc, char **new_argv[],
                     const char *bootstrapper_name,
                     const char *ldcs_location,
                     const char *ldcs_number,
                     unsigned long ldcs_options,
                     unsigned int test_launchers)
{
   int result = 0;
   int had_noexec = 0;
   int i;

   /* Presetup */
   if (test_launchers & TEST_PRESETUP) {
      /* If the bootstrapper is already present, don't do anything. Assume user has it right. */
      const char *bootstrapper_exec = strrchr(bootstrapper_name, '/');
      bootstrapper_exec = bootstrapper_exec ? bootstrapper_exec+1 : bootstrapper_name;
      for (i = 0; i < argc; i++) {
         if (strstr(argv[i], bootstrapper_exec)) {
            *new_argc = argc;
            *new_argv = argv;
            return 0;
         }
      }
   }

   /* Slurm */
   if (test_launchers & TEST_SLURM) {
      result = modifyCmdLineForLauncher(argc, argv, new_argc, new_argv, srun_options, srun_size,
                                        ldcs_location, ldcs_number, ldcs_options, bootstrapper_name);
      if (result == 0)
         return 0;
      else if (result == NO_EXEC)
         had_noexec = 1;
   }

   /* Add other launchers here */
   /* ... */
   
   /* Return the NO_EXEC error with priority over the NO_LAUNCHER */
   if (had_noexec)
      return NO_EXEC;
   return NO_LAUNCHER;
}
