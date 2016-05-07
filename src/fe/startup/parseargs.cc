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

#define GROUP_RELOC 1
#define GROUP_PUSHPULL 2
#define GROUP_NETWORK 3
#define GROUP_SEC 4
#define GROUP_LAUNCHER 5
#define GROUP_MISC 6

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

#if defined(os_bluegene)
#define DEFAULT_USE_SUBAUDIT 1
#else
#define DEFAULT_USE_SUBAUDIT 0
#endif

#define DEFAULT_PERSIST 0

static const char *YESNO = "yes|no";

static const unsigned long all_reloc_opts = OPT_RELOCAOUT | OPT_RELOCSO | OPT_RELOCEXEC | 
                                            OPT_RELOCPY | OPT_FOLLOWFORK;
static const unsigned long all_network_opts = OPT_COBO;
static const unsigned long all_pushpull_opts = OPT_PUSH | OPT_PULL;
static const unsigned long all_misc_opts = OPT_STRIP | OPT_DEBUG | OPT_PRELOAD | OPT_NOCLEAN | OPT_PERSIST;

static const unsigned long default_reloc_opts = OPT_RELOCAOUT | OPT_RELOCSO | OPT_RELOCEXEC | 
                                                OPT_RELOCPY | OPT_FOLLOWFORK;
static const unsigned long default_network_opts = OPT_COBO;
static const unsigned long default_pushpull_opts = OPT_PUSH;
static const unsigned long default_misc_opts = OPT_STRIP | (DEFAULT_PERSIST * OPT_PERSIST);
static const unsigned long default_sec = DEFAULT_SEC;

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

static unsigned long enabled_opts = 0;
static unsigned long disabled_opts = 0;

static char *preload_file;
static char **mpi_argv;
static int mpi_argc;
static bool done = false;
static bool hide_fd = true;
static int sec_model = -1;
static int launcher = 0;
static int startup_type = 0;
static int shm_cache_size = SHM_DEFAULT_SIZE;
static int use_subaudit = DEFAULT_USE_SUBAUDIT;
static const unsigned long persist = DEFAULT_PERSIST;


static set<string> python_prefixes;
static const char *default_python_prefixes = PYTHON_INST_PREFIX;
static char *user_python_prefixes = NULL;

#if DEFAULT_USE_SUBAUDIT == 1
#define DEFAULT_USE_SUBAUDIT_STR "subaudit"
#else
#define DEFAULT_USE_SUBAUDIT_STR "audit"
#endif

#if DEFAULT_PERSIST == 1
#define DEFAULT_PERSIST_STR "Yes"
#else
#define DEFAULT_PERSIST_STR "No"
#endif

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

unsigned long opts = 0;

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
#if defined(SECLMON)
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
   { "no-mpi", NOMPI, NULL, 0,
     "Run serial jobs instead of MPI job", GROUP_LAUNCHER },
   { NULL, 0, NULL, 0,
     "Misc options", GROUP_MISC },
   { "audit-type", AUDITTYPE, "subaudit|audit", 0,
     "Use the new-style subaudit interface for intercepting ld.so, or the old-style audit interface.  The subaudit option reduces memory overhead, but is more complex.  Default is " DEFAULT_USE_SUBAUDIT_STR ".", GROUP_MISC },
   { "shmcache-size", SHAREDCACHE_SIZE, "size", 0,
     "Size of client shared memory cache in kilobytes, which can be used to improve performance if multiple processes are running on each node.  Default: " STR(SHM_DEFAULT_SIZE), GROUP_MISC },
   { "python-prefix", PYTHONPREFIX, "path", 0,
     "Colon-seperated list of directories that contain the python install location", GROUP_MISC },
   { "debug", DEBUG, YESNO, 0,
     "If yes, hide spindle from debuggers so they think libraries come from the original locations.  May cause extra overhead. Default: no", GROUP_MISC },
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
   {0}
};

static int opt_key_to_code(int key)
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
      default: return 0;
   }
}

static bool multi_bits_set(unsigned long v)
{
   return (v & (v - 1)) != 0;
}

static int parse(int key, char *arg, struct argp_state *vstate)
{
   struct argp_state *state = (struct argp_state *) vstate;
   struct argp_option *entry;
   int opt = 0;

   if (done && key != ARGP_KEY_END)
      return 0;

   struct argp_option *last = options + (sizeof(options) / sizeof(struct argp_option)) - 1;
   for (entry = options; entry != last; entry++) {
      if (entry->key == key)
         break;
   }
   opt = opt_key_to_code(key);   

   if (entry->arg == YESNO) {
      if (strcmp(arg, "yes") == 0 || strcmp(arg, "y") == 0) {
         enabled_opts |= opt;
      }
      else if (strcmp(arg, "no") == 0 || strcmp(arg, "n") == 0)
         disabled_opts |= opt;
      else {
         argp_error(state, "%s must be 'yes' or 'no'", entry->name);
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
      }
      char *second_port = strchr(arg, '-');
      if (second_port == NULL) {
         num_ports = 1;
      }
      else {
         unsigned int port2 = atoi(second_port+1);
         if (spindle_port > port2) {
            argp_error(state, "port2 must be larger than port1");
         }
         if (spindle_port + 128 < port2) {
            argp_error(state, "port2 must be within 128 of port1");
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
      }
      if (shm_cache_size < 8) {
         argp_error(state, "shmcache-size argument must be at least 8 if non-zero");
      }
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
   else if (key == PYTHONPREFIX) {
      user_python_prefixes = arg;
      return 0;
   }
   else if (key == HOSTBIN) {
      hostbin_path = arg;
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
      }

      /* Set one and only one network option */
      unsigned long enabled_network_opts = enabled_opts & all_network_opts;
      if (multi_bits_set(enabled_network_opts)) {
         argp_error(state, "Cannot enable multiple network options");
      }
      opts |= enabled_network_opts ? enabled_network_opts : default_network_opts;

      /* Set one and only one push/pull option */
      unsigned long enabled_pushpull_opts = enabled_opts & all_pushpull_opts;
      if (multi_bits_set(enabled_pushpull_opts)) {
         argp_error(state, "Cannot enable both push and pull options");
      }
      opts |= enabled_pushpull_opts ? enabled_pushpull_opts : default_pushpull_opts;

      /* Set any reloc options */
      opts |= all_reloc_opts & ~disabled_opts & (enabled_opts | default_reloc_opts);

      /* Set startup type */
      if (launcher == serial_launcher)
         startup_type = startup_serial;
      else if (hostbin_path != NULL)
         startup_type = startup_hostbin;
      else
         startup_type = startup_lmon;

      /* Set security options */
      if (sec_model == -1)
         sec_model = default_sec;
      OPT_SET_SEC(opts, sec_model);

      /* Set any misc options */
      opts |= all_misc_opts & ~disabled_opts & (enabled_opts | default_misc_opts);
      opts |= use_subaudit ? OPT_SUBAUDIT : 0;
      opts |= logging_enabled ? OPT_LOGUSAGE : 0;
      opts |= shm_cache_size > 0 ? OPT_SHMCACHE : 0;

      return 0;
   }
   else if (key == ARGP_KEY_NO_ARGS) {
      argp_error(state, "No MPI command line found");
   }
   else {
      return 0;
   }
   assert(0);
   return -1;
}

unsigned long parseArgs(int argc, char *argv[])
{
   error_t result;
   argp_program_version = PACKAGE_VERSION;
   argp_program_bug_address = PACKAGE_BUGREPORT;
   
   done = false;
   struct argp arg_parser;
   bzero(&arg_parser, sizeof(arg_parser));
   arg_parser.options = options;
   arg_parser.parser = parse;
   arg_parser.args_doc = "mpi_command";
 
   result = argp_parse(&arg_parser, argc, argv, ARGP_IN_ORDER, NULL, NULL);
   assert(result == 0);

   if (opts & OPT_DEBUG) {
      //Debug mode overrides other settings
      opts &= ~OPT_RELOCAOUT;
      opts &= ~OPT_RELOCEXEC;
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

const string getLocation(int number)
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

unique_id_t get_unique_id()
{
   static unique_id_t unique_id = 0;
   if (unique_id != 0)
      return unique_id;

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
      fprintf(stderr, "Error: Could not open /dev/urandom or /dev/random for unique_id. Aborting Spindle\n");
      exit(-1);
   }
   uint32_t random;
   ssize_t result = read(fd, &random, sizeof(random));
   close(fd);
   if (result == -1) {
      fprintf(stderr, "Error: Could not read from /dev/urandom or /dev/random for unique_id. Aborting Spindle\n");
      exit(EXIT_FAILURE);
   }

   unique_id = (uint16_t) pid;
   unique_id |= ((uint64_t) hostname_hash) << 16;
   unique_id |= ((uint64_t) random) << 32;

   return unique_id;
}

void parseCommandLine(int argc, char *argv[], spindle_args_t *args)
{
   unsigned int opts = parseArgs(argc, argv);   

   args->number = getpid();
   args->port = getPort();
   args->num_ports = getNumPorts();
   args->opts = opts;
   args->unique_id = get_unique_id();
   args->use_launcher = getLauncher();
   args->startup_type = getStartupType();
   args->shm_cache_size = getShmCacheSize();
   args->location = strdup(getLocation(args->number).c_str());
   args->pythonprefix = strdup(getPythonPrefixes().c_str());
   args->preloadfile = getPreloadFile();

   debug_printf("Spindle options bitmask: %u\n", opts);
}

