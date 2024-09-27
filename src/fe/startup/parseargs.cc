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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <argp.h>
#include <cstring>
#include <cassert>
#include <stdlib.h>
#include <string>
#include <set>

using namespace std;

#include "config.h"
#include "spindle_launch.h"
#include "spindle_debug.h"
#include "parseargs.h"

#if !defined(STR)
#define STR2(X) #X
#define STR(X) STR2(X)
#endif

#define RELOCAOUT 'a'
#define SHAREDCACHE_SIZE 'b'
#define COBO 'c'
#define DEBUG 'd'
#define PRELOAD 'e'
#define FOLLOWFORK 'f'
#define HIDE 'h'
#define AUDITTYPE 'k'
#define RELOCSO 'l'
#define NOCLEAN 'n'
#define LOCATION 'o'
#define PUSH 'p'
#define PULL 'q'
#define PYTHONPREFIX 'r'
#define STRIP 's'
#define PORT 't'
#define NUMA 'u'
#define STOPRELOC 'w'
#define RELOCEXEC 'x'
#define RELOCPY 'y'
#define DISABLE_LOGGING 'z'
#define SECMUNGE (256+OPT_SEC_MUNGE)
#define SECKEYLMON (256+OPT_SEC_KEYLMON)
#define SECKEYFILE (256+OPT_SEC_KEYFILE)
#define SECNULL (256+OPT_SEC_NULL)
#define SLURM 270
#define OPENMPI 271
#define WRECK 272
#define NOMPI 273
#define HOSTBIN 274
#define PERSIST 275
#define STARTSESSION 276
#define RUNSESSION 277
#define ENDSESSION 278
#define LAUNCHERSTARTUP 279
#define MSGCACHE_BUFFER 280
#define MSGCACHE_TIMEOUT 281
#define CLEANUPPROC 282
#define RSHMODE 283

#define GROUP_RELOC 1
#define GROUP_PUSHPULL 2
#define GROUP_NETWORK 3
#define GROUP_SEC 4
#define GROUP_LAUNCHER 5
#define GROUP_SESSION 6
#define GROUP_MISC 7

#if defined(MUNGE)
#define DEFAULT_SEC OPT_SEC_MUNGE
#elif defined(SECLMON)
#define DEFAULT_SEC OPT_SEC_KEYFILE
#elif defined(KEYFILE)
#define DEFAULT_SEC OPT_SEC_KEYFILE
#elif defined(ENABLE_NULL_ENCRYPTION)
#define DEFAULT_SEC OPT_SEC_NULL
#else
#error No security model available
#endif

#if !defined(USE_SUBAUDIT_BY_DEFAULT)  /* May be defined via configure */
#if defined(os_bluegene)
#define DEFAULT_USE_SUBAUDIT 1
#else
#define DEFAULT_USE_SUBAUDIT 0
#endif
#else
#define DEFAULT_USE_SUBAUDIT 1
#endif

#if defined(DEFAULT_CLEANUP_PROC)
#define DEFAULT_CLEAN_PROC_STR "yes"
#define DEFAULT_CLEAN_PROC_INT 1
#else
#define DEFAULT_CLEAN_PROC_STR "no"
#define DEFAULT_CLEAN_PROC_INT 0
#endif

#define DEFAULT_PERSIST 0

static const char *YESNO = "yes|no";

static const opt_t all_reloc_opts = OPT_RELOCAOUT | OPT_RELOCSO | OPT_RELOCEXEC |
                                            OPT_RELOCPY | OPT_FOLLOWFORK | OPT_STOPRELOC;
static const opt_t all_network_opts = OPT_COBO;
static const opt_t all_pushpull_opts = OPT_PUSH | OPT_PULL;
static const opt_t all_misc_opts = OPT_STRIP | OPT_DEBUG | OPT_PRELOAD | OPT_NOCLEAN | OPT_PERSIST | OPT_PROCCLEAN;

static const opt_t default_reloc_opts = OPT_RELOCAOUT | OPT_RELOCSO | OPT_RELOCEXEC |
                                                OPT_RELOCPY | OPT_FOLLOWFORK;
static const opt_t default_network_opts = OPT_COBO;
static const opt_t default_pushpull_opts = OPT_PUSH;
static const opt_t default_misc_opts = OPT_STRIP | (DEFAULT_PERSIST * OPT_PERSIST) | OPT_DEBUG | (DEFAULT_CLEAN_PROC_INT * OPT_PROCCLEAN);
static const opt_t default_sec = DEFAULT_SEC;

#if defined(HOSTBIN_PATH)
static char default_hostbin_path[] = HOSTBIN_PATH;
static char *hostbin_path = default_hostbin_path;
#else
static char *hostbin_path = NULL;
#endif

#if defined(os_bluegene)
#define SHM_DEFAULT_SIZE 2048
#define SHM_MIN_SIZE 4
#else
#define SHM_DEFAULT_SIZE 0
#define SHM_MIN_SIZE 0
#endif

#define DEFAULT_MSGCACHE_BUFFER_KB 1024
#define DEFAULT_MSGCACHE_TIMEOUT_MS 100
#define DEFAULT_MSGCACHE_ON 0

static opt_t enabled_opts = 0;
static opt_t disabled_opts = 0;

static char *preload_file;
static char **mpi_argv;
static int mpi_argc;
static bool done = false;
static bool hide_fd = true;
static int sec_model = -1;
static int launcher = 0;
static int startup_type = 0;
static int shm_cache_size = SHM_DEFAULT_SIZE;
static opt_t use_subaudit = DEFAULT_USE_SUBAUDIT;
static int msgcache_buffer_kb = DEFAULT_MSGCACHE_BUFFER_KB;
static int msgcache_timeout_ms = DEFAULT_MSGCACHE_TIMEOUT_MS;
static int msgcache_set = DEFAULT_MSGCACHE_ON;

static session_status_t session_status = sstatus_unused;
static string session_id;

static set<string> python_prefixes;
static const char *default_python_prefixes = PYTHON_INST_PREFIX;
static char *user_python_prefixes = NULL;

static char *numa_substrings = NULL;

#if DEFAULT_USE_SUBAUDIT == 1
#define DEFAULT_USE_SUBAUDIT_STR "subaudit"
#else
#define DEFAULT_USE_SUBAUDIT_STR "audit"
#endif

#if defined(HAVE_LMON)
#define DEFAULT_MPI_STARTUP startup_lmon
#else
#define DEFAULT_MPI_STARTUP startup_mpi
#endif

#if DEFAULT_PERSIST == 1
#define DEFAULT_PERSIST_STR "Yes"
#else
#define DEFAULT_PERSIST_STR "No"
#endif

#if defined(RSHLAUNCH_ENABLED)
#define DEFAULT_RSHMODE 1
#define DEFAULT_RSHMODE_STR "Yes"
#else
#define DEFAULT_RSHMODE 0
#define DEFAULT_RSHMODE_STR "No"
#endif
static int use_rsh = DEFAULT_RSHMODE;

#if defined(USAGE_LOGGING_FILE)
#define DEFAULT_LOGGING_ENABLED true
static const int DISABLE_LOGGING_FLAGS = 0;
#else
#define DEFAULT_LOGGING_ENABLED false
static const int DISABLE_LOGGING_FLAGS = OPTION_HIDDEN;
#endif
static bool logging_enabled = DEFAULT_LOGGING_ENABLED;

static unsigned int spindle_port = SPINDLE_PORT;
static unsigned int num_ports = NUM_COBO_PORTS;

string spindle_location(SPINDLE_LOC);

opt_t opts = 0;

struct argp_option options[] = {
   { NULL, 0, NULL, 0,
     "These options specify what types of files should be loaded through the Spindle network" },
   { "reloc-aout", RELOCAOUT, YESNO, 0, 
     "Relocate the main executable through Spindle. Default: yes", GROUP_RELOC },
   { "reloc-libs", RELOCSO, YESNO, 0,
     "Relocate shared libraries through Spindle. Default: yes", GROUP_RELOC },
   { "reloc-python", RELOCPY, YESNO, 0,
     "Relocate python modules (.py/.pyc) files when loaded via python. Default: yes", GROUP_RELOC },
   { "reloc-exec", RELOCEXEC, YESNO, 0,
     "Relocate the targets of exec/execv/execve/... calls. Default: yes", GROUP_RELOC },
   { "follow-fork", FOLLOWFORK, YESNO, 0,
     "Relocate objects in fork'd child processes. Default: yes", GROUP_RELOC },
   { "stop-reloc", STOPRELOC, YESNO, 0,
     "Do not relocate file contents, though still intercept operations that would lead to file-not-found returns. Default: no", GROUP_RELOC },
   { NULL, 0, NULL, 0,
     "These options specify how the Spindle network should distibute files.  Push is better for SPMD programs.  Pull is better for MPMD programs. Default is push.", GROUP_PUSHPULL },
   { "push", PUSH, NULL, 0,
     "Use a push model where objects loaded by any process are made available to all processes", GROUP_PUSHPULL },
   { "pull", PULL, NULL, 0,
     "Use a pull model where objects are only made available to processes that require them", GROUP_PUSHPULL },
   { NULL, 0, NULL, 0,
     "These options configure Spindle's network model.  Typical Spindle runs should not need to set these.", GROUP_NETWORK },
   { "cobo", COBO, NULL, 0,
     "Use a tree-based cobo network for distributing objects", GROUP_NETWORK },
   { "port", PORT, "port1-port2", 0,
     "TCP/IP port range for Spindle servers.  Default: " STR(SPINDLE_PORT) "-" STR(SPINDLE_MAX_PORT), GROUP_NETWORK },
   { NULL, 0, NULL, 0,
     "These options specify the security model Spindle should use for validating TCP connections. "
     "Spindle will choose a default value if no option is specified.", GROUP_SEC },
#if defined(MUNGE)
   { "security-munge", SECMUNGE, NULL, 0,
     "Use munge for security authentication", GROUP_SEC },
#endif
#if defined(SECLMON) && defined(HAVE_LMON)
   { "security-lmon", SECKEYLMON, NULL, 0,
     "Use LaunchMON to exchange keys for security authentication", GROUP_SEC },
#endif
#if defined(KEYFILE)
   { "security-keyfile", SECKEYFILE, NULL, 0,
     "Use a keyfile stored in a global file system for security authentication", GROUP_SEC },
#endif
#if defined(ENABLE_NULL_ENCRYPTION)
   { "security-none", SECNULL, NULL, 0,
     "Do not do any security authentication", GROUP_SEC },
#endif
   { NULL, 0, NULL, 0,
     "These options specify the job launcher Spindle is being run with.  If unspecified, Spindle will try to autodetect.", GROUP_LAUNCHER },
#if defined(ENABLE_SRUN_LAUNCHER)
   { "slurm", SLURM, NULL, 0,
     "MPI job is launched with the srun job launcher.", GROUP_LAUNCHER },
#endif
#if defined(ENABLE_OPENMPI_LAUNCHER)
   { "openmpi", OPENMPI, NULL, 0,
     "MPI job is launched with the OpenMPI job jauncher.", GROUP_LAUNCHER },
#endif
#if defined(ENABLE_WRECKRUN_LAUNCHER)
   { "wreck", WRECK, NULL, 0,
     "MPI Job is launched with the wreck job launcher.", GROUP_LAUNCHER },
#endif
#if defined(ENABLE_SRUN_LAUNCHER)
   { "launcher-startup", LAUNCHERSTARTUP, NULL, 0,
     "Launch spindle daemons using the system's job launcher (requires an already set-up session).", GROUP_LAUNCHER },
#endif
   { "no-mpi", NOMPI, NULL, 0,
     "Run serial jobs instead of MPI job", GROUP_LAUNCHER },
   { NULL, 0, NULL, 0,
     "Options for managing sessions, which can run multiple jobs out of one spindle cache.", GROUP_SESSION },
   { "start-session", STARTSESSION, NULL, 0,
     "Start a persistent Spindle session and print the session-id to stdout", GROUP_SESSION },
   { "end-session", ENDSESSION, "session-id", 0,
     "End a persistent Spindle session with the given session-id", GROUP_SESSION },
   { "run-in-session", RUNSESSION, "session-id", 0,
     "Run a new job in the given session", GROUP_SESSION },
   { NULL, 0, NULL, 0,
     "Misc options", GROUP_MISC },
#if defined(LIBNUMA)
   { "numa", NUMA, "list", OPTION_ARG_OPTIONAL,
     "Colon-seperated list of substrings that will be matched to executables/libraries. Matches will have their memory replicated into each NUMA domain."
     " Specify the option, but leave it blank to replicate all spindle-relocated files into each NUMA domain", GROUP_MISC },
#endif
   { "audit-type", AUDITTYPE, "subaudit|audit", 0,
     "Use the new-style subaudit interface for intercepting ld.so, or the old-style audit interface.  The subaudit option reduces memory overhead, but is more complex.  Default is " DEFAULT_USE_SUBAUDIT_STR ".", GROUP_MISC },
   { "shmcache-size", SHAREDCACHE_SIZE, "size", 0,
     "Size of client shared memory cache in kilobytes, which can be used to improve performance if multiple processes are running on each node.  Default: " STR(SHM_DEFAULT_SIZE), GROUP_MISC },
   { "python-prefix", PYTHONPREFIX, "path", 0,
     "Colon-seperated list of directories that contain the python install location", GROUP_MISC },
   { "cache-prefix", PYTHONPREFIX, "path", 0,
     "Alias for python-prefix" },
   { "debug", DEBUG, YESNO, 0,
     "If yes, hide spindle from debuggers so they think libraries come from the original locations.  May cause extra overhead. Default: yes", GROUP_MISC },
   { "hostbin", HOSTBIN, "EXECUTABLE", 0,
     "Path to a script that returns the hostlist for a job on a cluster", GROUP_MISC },
   { "preload", PRELOAD, "FILE", 0,
     "Provides a text file containing a white-space separated list of files that should be "
     "relocated to each node before execution begins", GROUP_MISC },
   { "strip", STRIP, YESNO, 0,
     "Strip debug and symbol information from binaries before distributing them. Default: yes", GROUP_MISC },
   { "location", LOCATION, "directory", 0,
     "Back-end directory for storing relocated files.  Should be a non-shared location such as a ramdisk.  Default: " SPINDLE_LOC, GROUP_MISC },
   { "noclean", NOCLEAN, YESNO, 0,
     "Don't remove local file cache after execution.  Default: no (removes the cache)", GROUP_MISC },
   { "disable-logging", DISABLE_LOGGING, NULL, DISABLE_LOGGING_FLAGS,
     "Disable usage logging for this invocation of Spindle", GROUP_MISC },
   { "no-hide", HIDE, NULL, 0,
     "Don't hide spindle file descriptors from application", GROUP_MISC },
   { "persist", PERSIST, YESNO, 0,
     "Allow spindle servers to persist after the last client job has exited. Default: " DEFAULT_PERSIST_STR, GROUP_MISC },
   { "msgcache-buffer", MSGCACHE_BUFFER, "size", 0,
     "Enables message buffering if size is non-zero, otherwise sets the size of the buffer in kilobytes", GROUP_MISC },
   { "msgcache-timeout", MSGCACHE_TIMEOUT, "timeout", 0,
     "Enables message buffering if size is non-zero, otherwise sets the buffering timeout in milliseconds", GROUP_MISC },
   { "cleanup-proc", CLEANUPPROC, YESNO, 0,
     "Fork a dedicated process to clean-up files post-spindle.  Useful for high-fault situations. Default: " DEFAULT_CLEAN_PROC_STR, GROUP_MISC },
   { "enable-rsh", RSHMODE, YESNO, 0,
     "Enable startint daemons with an rsh tree, if the startup mode supports it. Default: " DEFAULT_RSHMODE_STR, GROUP_MISC },
   {0}
};

static opt_t opt_key_to_code(int key)
{
   switch (key) {
      case RELOCAOUT: return OPT_RELOCAOUT;
      case COBO: return OPT_COBO;
      case DEBUG: return OPT_DEBUG;
      case PRELOAD: return OPT_PRELOAD;
      case FOLLOWFORK: return OPT_FOLLOWFORK;
      case RELOCSO: return OPT_RELOCSO;
      case PUSH: return OPT_PUSH;
      case PULL: return OPT_PULL;
      case STRIP: return OPT_STRIP;
      case RELOCEXEC: return OPT_RELOCEXEC;
      case RELOCPY: return OPT_RELOCPY;
      case NOCLEAN: return OPT_NOCLEAN;
      case PERSIST: return OPT_PERSIST;
      case CLEANUPPROC: return OPT_PROCCLEAN;
      case STOPRELOC: return OPT_STOPRELOC;
      default: return 0;
   }
}

static bool multi_bits_set(opt_t v)
{
   return (v & (v - 1)) != 0;
}

static int parse(int key, char *arg, struct argp_state *vstate)
{
   struct argp_state *state = (struct argp_state *) vstate;
   struct argp_option *entry;
   int msgbuffer_arg;
   opt_t opt = 0;

   state->err_stream = (FILE *) state->input;
   state->out_stream = (FILE *) state->input;

   if (done && key != ARGP_KEY_END)
      return 0;

   struct argp_option *last = options + (sizeof(options) / sizeof(struct argp_option)) - 1;
   for (entry = options; entry != last; entry++) {
      if (entry->key == key)
         break;
   }
   opt = opt_key_to_code(key);   

   if (entry->arg == YESNO && opt) {
      if (strcmp(arg, "yes") == 0 || strcmp(arg, "y") == 0) {
         enabled_opts |= opt;
      }
      else if (strcmp(arg, "no") == 0 || strcmp(arg, "n") == 0)
         disabled_opts |= opt;
      else {
         argp_error(state, "%s must be 'yes' or 'no'", entry->name);
         return ARGP_ERR_UNKNOWN;
      }
      return 0;
   }
   else if (entry->key && entry->arg == NULL && opt) {
      enabled_opts |= opt;
      return 0;
   }
   else if (entry->key == PRELOAD) {
      enabled_opts |= opt;
      preload_file = arg;
      return 0;
   }
   else if (entry->key == PORT) {
      spindle_port = atoi(arg);
      if (!spindle_port) {
         argp_error(state, "Port was given a 0 value");
         return ARGP_ERR_UNKNOWN;         
      }
      char *second_port = strchr(arg, '-');
      if (second_port == NULL) {
         num_ports = 1;
      }
      else {
         unsigned int port2 = atoi(second_port+1);
         if (spindle_port > port2) {
            argp_error(state, "port2 must be larger than port1");
            return ARGP_ERR_UNKNOWN;
         }
         if (spindle_port + 128 < port2) {
            argp_error(state, "port2 must be within 128 of port1");
            return ARGP_ERR_UNKNOWN;
         }
         num_ports = port2 - spindle_port + 1;
      }
      return 0;
   }
   else if (entry->key == SHAREDCACHE_SIZE) {
      shm_cache_size = atoi(arg);
      if (shm_cache_size < SHM_MIN_SIZE)
         shm_cache_size = SHM_MIN_SIZE;
      if (shm_cache_size % 4 != 0) {
         argp_error(state, "shmcache-size argument must be a multiple of 4");
         return ARGP_ERR_UNKNOWN;
      }
      if (shm_cache_size < 8) {
         argp_error(state, "shmcache-size argument must be at least 8 if non-zero");
         return ARGP_ERR_UNKNOWN;
      }
      return 0;
   }
   else if (entry->key == MSGCACHE_BUFFER || entry->key == MSGCACHE_TIMEOUT) {
      if (!arg) {
         argp_error(state, "msgcache settings must have an argument");
         return ARGP_ERR_UNKNOWN;
      }
      msgbuffer_arg = atoi(arg);
      if (!msgbuffer_arg) {
         msgcache_set = 0;
         return 0;
      }
      msgcache_set = 1;
      if (entry->key == MSGCACHE_BUFFER)
         msgcache_buffer_kb = msgbuffer_arg;
      else
         msgcache_timeout_ms = msgbuffer_arg;
      return 0;
   }
   else if (entry->key == AUDITTYPE) {
      if (strcmp(arg, "subaudit") == 0) {
         use_subaudit = 1;
      }
      else if (strcmp(arg, "audit") == 0) {
         use_subaudit = 0;
      }
      else {
         argp_error(state, "audit-type argument must be 'audit' or 'subaudit'");
         return ARGP_ERR_UNKNOWN;
      }
      return 0;
   }
   else if (entry->key == LOCATION) {
      spindle_location = arg;
      return 0;
   }
   else if (key == DISABLE_LOGGING) {
      logging_enabled = false;
      return 0;
   }
   else if (key == SLURM) {
      launcher = srun_launcher;
      return 0;
   }
   else if (key == OPENMPI) {
      launcher = openmpi_launcher;
      return 0;
   }
   else if (key == WRECK) {
      launcher = wreckrun_launcher;
      return 0;
   }
   else if (key == NOMPI) {
      launcher = serial_launcher;
      return 0;
   }
   else if (key == HIDE) {
      hide_fd = false;
      opts |= OPT_NOHIDE;
      return 0;
   }
   else if (entry->group == GROUP_SEC) {
      sec_model = key - 256;
      return 0;
   }
   else if (key == ARGP_KEY_ARG) {
      if (state->argc == 0) {
         return 0;
      }
      return ARGP_ERR_UNKNOWN;
   }
   else if (key == NUMA) {
      opts |= OPT_NUMA;
      numa_substrings = arg;
      return 0;
   }
   else if (key == PYTHONPREFIX) {
      user_python_prefixes = arg;
      return 0;
   }
   else if (key == HOSTBIN) {
      hostbin_path = arg;
      return 0;
   }
   else if (key == STARTSESSION) {
      session_status = sstatus_start;
      opts |= OPT_SESSION;
      return 0;
   }
   else if (key == RUNSESSION) {
      session_status = sstatus_run;
      session_id = string(arg);
      opts |= OPT_SESSION;
      return 0;
   }
   else if (key == ENDSESSION) {
      session_status = sstatus_end;
      session_id = string(arg);
      opts |= OPT_SESSION;
      return 0;
   }
   else if (key == LAUNCHERSTARTUP) {
      startup_type = startup_mpi;
#if defined(TESTRM)
      if (strcmp(TESTRM, "slurm") == 0)
         launcher = srun_launcher;
#endif
      return 0;
   }
   else if (entry->key == RSHMODE) {
      if (strcmp(arg, "yes") == 0 || strcmp(arg, "y") == 0) {
         use_rsh = 1;
      }
      else if (strcmp(arg, "no") == 0 || strcmp(arg, "n") == 0) {
         use_rsh = 0;
      }
      else {
         argp_error(state, "%s must be 'yes' or 'no'", entry->name);
         return ARGP_ERR_UNKNOWN;
      }
      return 0;
   }   
   else if (key == ARGP_KEY_ARGS) {
      mpi_argv = state->argv + state->next;
      mpi_argc = state->argc - state->next;
      done = true;
      return 0;
   }
   else if (key == ARGP_KEY_END) {
      if (enabled_opts & disabled_opts) {
         argp_error(state, "Cannot have the same option both enabled and disabled");
         return ARGP_ERR_UNKNOWN;
      }

      /* Set one and only one network option */
      opt_t enabled_network_opts = enabled_opts & all_network_opts;
      if (multi_bits_set(enabled_network_opts)) {
         argp_error(state, "Cannot enable multiple network options");
         return ARGP_ERR_UNKNOWN;
      }
      opts |= enabled_network_opts ? enabled_network_opts : default_network_opts;

      /* Set one and only one push/pull option */
      opt_t enabled_pushpull_opts = enabled_opts & all_pushpull_opts;
      if (multi_bits_set(enabled_pushpull_opts)) {
         argp_error(state, "Cannot enable both push and pull options");
         return ARGP_ERR_UNKNOWN;
      }
      opts |= enabled_pushpull_opts ? enabled_pushpull_opts : default_pushpull_opts;

      /* Set any reloc options */
      opts |= all_reloc_opts & ~disabled_opts & (enabled_opts | default_reloc_opts);

      /* Set startup type */
      if (startup_type == 0) {
         if (launcher == serial_launcher)
            startup_type = startup_serial;
         else if (hostbin_path != NULL) {
            startup_type = startup_hostbin;
            opts |= OPT_SELFLAUNCH;
         }
         else
            startup_type = DEFAULT_MPI_STARTUP;
      }

      /* Set security options */
      if (sec_model == -1)
         sec_model = default_sec;
      OPT_SET_SEC(opts, sec_model);

      /* Set any misc options */
      opts |= all_misc_opts & ~disabled_opts & (enabled_opts | default_misc_opts);
      opts |= use_subaudit ? OPT_SUBAUDIT : 0;
      opts |= logging_enabled ? OPT_LOGUSAGE : 0;
      opts |= shm_cache_size > 0 ? OPT_SHMCACHE : 0;

      /* Set message buffer options */
      if (msgcache_set) {
         opts |= OPT_MSGBUNDLE;
         if (!msgcache_buffer_kb)
            msgcache_buffer_kb = DEFAULT_MSGCACHE_BUFFER_KB;
         if (!msgcache_timeout_ms)
            msgcache_timeout_ms = DEFAULT_MSGCACHE_TIMEOUT_MS;
      }
      else {
         msgcache_buffer_kb = 0;
         msgcache_timeout_ms = 0;
      }
           
      /* Set session options */
      if (opts & OPT_SESSION) { 
         opts |= OPT_PERSIST;
         if (startup_type == startup_lmon || startup_type == startup_hostbin) {
            debug_printf("Changing unsupported startup type in session-mode to startup_mpi\n");
            startup_type = startup_mpi;
         }
      }
     
      return 0;
   }
   else if (key == ARGP_KEY_NO_ARGS && 
            !(session_status == sstatus_start || session_status == sstatus_end)) {
      argp_error(state, "No MPI command line found");
      return ARGP_ERR_UNKNOWN;
   }
   else {
      return 0;
   }
   assert(0);
   return -1;
}

opt_t parseArgs(int argc, char *argv[], unsigned int flags, FILE *io)
{
   error_t result;
   unsigned int argflags = 0;
   struct argp arg_parser;
   
   argp_program_version = PACKAGE_VERSION;
   argp_program_bug_address = PACKAGE_BUGREPORT;
   
   done = false;
   bzero(&arg_parser, sizeof(arg_parser));
   arg_parser.options = options;
   arg_parser.parser = parse;
   arg_parser.args_doc = "mpi_command";
   argflags = ARGP_IN_ORDER;
   argflags |= flags & PARSECMD_FLAG_NOEXIT ? ARGP_NO_EXIT : 0;
   
   result = argp_parse(&arg_parser, argc, argv, argflags, NULL, io);
   if (result != 0)
      return (opt_t) 0;

   if (opts & OPT_DEBUG) {
      //Debug mode overrides other settings
      opts |= OPT_REMAPEXEC;
   }

   return opts;
}

char *getPreloadFile()
{
   return preload_file;
}

unsigned int getPort()
{
   return spindle_port;
}

string getLocation(int number)
{
   char num_s[32];
   snprintf(num_s, 32, "%d", number);
   return spindle_location + string("/spindle.") + string(num_s);
}

static void parse_python_prefix(const char *prefix)
{
   if (!prefix)
      return;

   const char *cur = prefix;
   for (;;) {
      const char *colon;
      for (colon = cur; *colon != '\0' && *colon != ':'; colon++);
      string s(cur, colon - cur);
      if (!s.empty())
         python_prefixes.insert(s);

      if (*colon == '\0')
         break;
      cur = colon + 1;
   }
}

string getPythonPrefixes()
{
   parse_python_prefix(default_python_prefixes);
   parse_python_prefix(user_python_prefixes);

   string result;
   set<string>::iterator i = python_prefixes.begin();
   for (;;) {
      result += *i;
      i++;
      if (i == python_prefixes.end())
         break;
      result += ":";
   }
   return result;
}

int getAppArgs(int *argc, char ***argv)
{
   *argc = mpi_argc;
   *argv = mpi_argv;
   return 0;
}

int getLauncher()
{
   return launcher;
}

std::string getHostbin()
{
   return string(hostbin_path);
}

int getStartupType()
{
   return startup_type;
}

unsigned int getNumPorts()
{
   return num_ports;
}

int getShmCacheSize() 
{
   return shm_cache_size;
}

static unsigned int str_hash(const char *str)
{
   unsigned long hash = 5381;
   int c;
   while ((c = *str++))
      hash = ((hash << 5) + hash) + c;
   return (unsigned int) hash;
}

unique_id_t get_unique_id(FILE *err_io)
{
   static unique_id_t unique_id = 0;
   if (unique_id != 0) {
      return unique_id;
   }

   /* This needs only needs to be unique between overlapping Spindle instances
      actively running on the same node.  Grab 16-bit pid, 16-bits hostname hash,
      32-bits randomness */
      
   uint16_t pid = (uint16_t) getpid();
   
   char hostname[256];
   gethostname(hostname, sizeof(hostname));
   hostname[sizeof(hostname)-1] = '\0';
   uint16_t hostname_hash = (uint16_t) str_hash(hostname);

   int fd = open("/dev/urandom", O_RDONLY);
   if (fd == -1)
      fd = open("/dev/random", O_RDONLY);
   if (fd == -1) {
      fprintf(err_io, "Error: Could not open /dev/urandom or /dev/random for unique_id. Aborting Spindle\n");      
      return 0;
   }
   uint32_t random;
   ssize_t result = read(fd, &random, sizeof(random));
   close(fd);
   if (result == -1) {
      fprintf(err_io, "Error: Could not read from /dev/urandom or /dev/random for unique_id. Aborting Spindle\n");            
      return 0;
   }

   unique_id = (uint16_t) pid;
   unique_id |= ((uint64_t) hostname_hash) << 16;
   unique_id |= ((uint64_t) random) << 32;
   if (!unique_id)
      unique_id = 1;
   
   return unique_id;
}

int parseCommandLine(int argc, char *argv[], spindle_args_t *args, unsigned int flags, char **errstring)
{
   opt_t opts;
   FILE *io = NULL;
   char *captured_io = NULL;
   size_t captured_io_size = 0;
   int result = -1;
   
   if (flags & PARSECMD_FLAG_CAPTUREIO) {
      io = open_memstream(&captured_io, &captured_io_size);
   }
   if (!io) {
      io = stderr;
   }
   
   opts = parseArgs(argc, argv, flags, io);
   if (opts == (opt_t) 0)
      goto done;

   if (!(flags & PARSECMD_FLAG_NONUMBER))
      args->number = getpid();
   args->port = getPort();
   args->num_ports = getNumPorts();
   args->opts = opts;
   args->use_launcher = getLauncher();
   args->startup_type = getStartupType();
   args->shm_cache_size = getShmCacheSize();
   args->location = strdup(getLocation(args->number).c_str());
   args->pythonprefix = strdup(getPythonPrefixes().c_str());
   args->preloadfile = getPreloadFile();
   args->bundle_timeout_ms = msgcache_timeout_ms;
   args->bundle_cachesize_kb = msgcache_buffer_kb;
   args->numa_files = numa_substrings ? strdup(numa_substrings) : NULL;
      
   args->unique_id = 0;
   if (!(flags & PARSECMD_FLAG_NOUNIQUEID)) {
      args->unique_id = get_unique_id(io);
      if (!args->unique_id)
         goto done;
   }

   debug_printf("Spindle options bitmask: %lu\n", (unsigned long) opts);
   result = 0;
  done:
   if (io && io != stderr)
      fclose(io);
   
   if (errstring && captured_io_size)
      *errstring = captured_io;
   else if (errstring && !captured_io_size)
      *errstring = NULL;
   else if (!errstring && captured_io_size)
      free(captured_io);

   return result;   
}

string get_arg_session_id()
{
   assert(session_status == sstatus_run || session_status == sstatus_end);
   return session_id;
}

session_status_t get_session_status()
{
   return session_status;
}

int getUseRSH()
{
   return use_rsh;
}

char *get_numa_substrings()
{
   return numa_substrings;
}
