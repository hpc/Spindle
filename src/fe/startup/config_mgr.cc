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

#include "config_mgr.h"
#include "config_parser.h"
#include "spindle_debug.h"
#include "config.h"

#include <sstream>
#include <functional>

#include <cassert>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

using namespace std;

#if !defined(PKGSYSCONFDIR)
#error need PKGSYSCONFDIR
#endif

#if !defined(STR)
#define STR2(X) #X
#define STR(X) STR2(X)
#endif

#if defined(SPINDLE_PORT)
#define SPINDLE_PORT_STR STR(SPINDLE_PORT)
#else
#define SPINDLE_PORT_STR "21940"
#endif

#if defined(NUM_COBO_PORTS)
#define SPINDLE_NUM_PORTS_STR STR(NUM_COBO_PORTS)
#else
#define SPINDLE_NUM_PORTS_STR "250"
#endif

#if defined(COMMPATH)
#define SPINDLE_COMMPATH_STR COMMPATH
#else
#define SPINDLE_COMMPATH_STR "$TMPDIR"
#endif

#if defined(CACHEPATHS)
#define SPINDLE_CACHEPATHS_STR CACHEPATHS
#else
#define SPINDLE_CACHEPATHS_STR "$TMPDIR"
#endif

#if defined(SPINDLE_LOCAL_PREFIX)
#define SPINDLE_LOCAL_PREFIX_STR SPINDLE_LOCAL_PREFIX
#else
#define SPINDLE_LOCAL_PREFIX_STR "/var/:/tmp/:/proc/:/dev/:/sys/"
#endif

#if defined(TESTRM)
#  define DEFAULT_LAUNCHER_STR TESTRM
#else
#  if ENABLE_FLUX_PLUGIN
#     define DEFAULT_LAUNCHER_STR "flux"
#  elif ENABLE_SLURM_PLUGIN
#     define DEFAULT_LAUNCHER_STR "slurm-plugin"
#  elif ENABLE_SLURM
#     define DEFAULT_LAUNCHER_STR "slurm"
#  else
#     define DEFAULT_LAUNCHER_STR "serial"
#  endif
#endif

#if defined(MUNGE)
#define SECURITY_DEFAULT_STR "munge"
#elif defined(KEYFILE)
#define SECURITY_DEFAULT_STR "keyfile"
#elif defined(SECLMON)
#define SECURITY_DEFAULT_STR "launchmon"
#else
#define SECURITY_DEFAULT_STR "none"
#endif

#if defined(LIBNUMA) && defined(NUMA_EXCLUDES)
#define NUMA_EXCLUDES_STR NUMA_EXCLUDES
#else
#define NUMA_EXCLUDES_STR ""
#endif

#if defined(USE_SUBAUDIT_BY_DEFAULT)
#define AUDITTYPE_STR "subaudit"
#else
#define AUDITTYPE_STR "audit"
#endif

#if defined(os_bluegene)
#define SHMCACHE_SIZE_STR "2048"
#else
#define SHMCACHE_SIZE_STR "0"
#endif

#if defined(HOSTBIN_PATH)
#define HOSTBIN_ENABLE_STR "true"
#define HOSTBIN_PATH_STR HOSTBIN_PATH
#else
#define HOSTBIN_ENABLE_STR "false"
#define HOSTBIN_PATH_STR ""
#endif

#if defined(USAGE_LOGGING_FILE)
#define DISABLE_LOGGING_STR "false"
#else
#define DISABLE_LOGGING_STR "true"
#endif

#if defined(DEFAULT_CLEANUP_PROC)
#define CLEANUP_PROC_STR "true"
#else
#define CLEANUP_PROC_STR "false"
#endif

#if defined(RSHLAUNCH_ENABLED)
#define RSHLAUNCH_STR "true"
#else
#define RSHLAUNCH_STR "false"
#endif

#if defined(RSHLAUNCH_CMD)
#define RSHCMD_STR RSHLAUNCH_CMD
#else
#define RSHCMD_STR ""
#endif

#if defined(PYTHON_INST_PREFIX)
#define PYTHON_PREFIX_DEFAULT PYTHON_INST_PREFIX
#else
#define PYTHON_PREFIX_DEFAULT ""
#endif

#if BINARY_PATCH_LDSO == 1
#define DEFAULT_PATCH_LDSO "true"
#else
#define DEFAULT_PATCH_LDSO "false"
#endif

#define DEFAULT_EXEC_EXCLUDE_LIST "gcc:g++:cc:CC:clang:clang++:hipcc:amdclang:amdclang++:craycc:crayCC:ld:ld.lld:mpicc:mpic++:mpicxx:make:gmake:cmake"
const list<SpindleOption> *Options;

void initOptionsList()
{
   if (Options)
      return;
   
   Options = new list<SpindleOption>(
   {
   { confCmdlineNewgroup, "", shortNone, groupReloc, cvBool, {}, "",
     "These options specify what types of files should be loaded through the Spindle network" },
   { confRelocAout, "reloc-aout", shortRelocAout, groupReloc, cvBool, {}, "true",
     "Relocate the main executable through Spindle." },
   { confRelocLibs, "reloc-libs", shortRelocSO, groupReloc, cvBool, {}, "true",
     "Relocate shared libraries through Spindle." },
   { confRelocPython, "reloc-python", shortRelocPy, groupReloc, cvBool, {}, "true",
     "Relocate python modules (.py/.pyc) files when loaded via python." },
   { confRelocExec, "reloc-exec", shortRelocExec, groupReloc, cvBool, {}, "true",
     "Relocate the targets of exec/execv/execve/... calls." },
   { confFollowFork, "follow-fork", shortFollowFork, groupReloc, cvBool, {}, "true",
     "Relocate objects in fork'd child processes." },
   { confStopReloc, "stop-reloc", shortStopReloc, groupReloc, cvBool, {}, "false",
     "Do not relocate file contents, though still intercept operations that would lead to file-not-found returns." },
   { confSpindleLevel, "level", shortSpindleLevel, groupReloc, cvEnum, { "high", "medium", "low", "off", "default" }, "default",
     "Sets the other spindle relocation options on/off based on the specified level. 'high' relocates everything possible. 'medium' is a balance "
     "that tries for minimal impact on debugging. 'low' turns off most relocation, but leaves a file existance cache. 'off' turns off spindle." },

   { confCmdlineNewgroup, "", shortNone, groupPushPull, cvBool, {}, "",
     "These options specify how the Spindle network should distibute files. Push is better for SPMD programs. Pull is better for MPMD programs. Default is push." },
   { confCmdlineOnly, "push", shortPush, groupPushPull, cvBool, {}, "true",
     "Use a push model where objects loaded by any process are made available to all processes. (Deprecated for --pushpull=push)" },
   { confCmdlineOnly, "pull", shortPull, groupPushPull, cvBool, {}, "false",
     "Use a pull model where objects are only made available to processes that require them. (Deprecated for --pushpull=pull)" },
   { confPushpull, "pushpull", shortPushPull, groupPushPull, cvEnum, { "push", "pull" }, "push",
     "Sets the network 'push' or 'pull' model to the given string." },

   { confCmdlineNewgroup, "", shortNone, groupNetwork, cvBool, {}, "",
     "These options configure Spindle's network model.  Typical Spindle runs should not need to set these." },
   { confCmdlineOnly, "cobo", shortCobo, groupNetwork, cvBool, {}, "true",
     "Use a tree-based cobo network for distributing objects. (Deprecated for --network=cobo)" },
   { confNetwork, "network", shortNetwork, groupNetwork, cvEnum, { "cobo" }, "cobo",
     "Sets the network communication type to the given mode." },
   { confPort, "port", shortPort, groupNetwork, cvInteger, {}, SPINDLE_PORT_STR,
     "The first port to use for spindle network communication." },
   { confNumPorts, "num-ports", shortNumPorts, groupNetwork, cvInteger, {}, SPINDLE_NUM_PORTS_STR,
     "The number of ports to fall back to if the initial port is in use." },

   { confCmdlineNewgroup, "", shortNone, groupSec, cvBool, {}, "",
     "These options specify the security model Spindle should use for validating TCP connections." },
   { confCmdlineOnly, "security-munge", shortSecMunge, groupSec, cvBool, {}, "",
     "Use munge for security authentication. (Deprecated for --security=munge)" },
   { confCmdlineOnly, "security-lmon", shortSecKeyLMon, groupSec, cvBool, {}, "",
     "Use launchmon to exchange keys for security authentication. (Deprecated for --security=launchmon)" },
   { confCmdlineOnly, "security-keyfile", shortSecKeyFile, groupSec, cvBool, {}, "",
     "Use a keyfile stored in a global file system for security authentication. (Deprecated for --security=keyfile)" },
   { confCmdlineOnly, "security-none", shortSecNull, groupSec, cvBool, {}, "",
     "Do not do any security authentication. (Deprecated for --security=none)" },
   { confSecurity, "security", shortSecuritySet, groupSec, cvEnum, { "munge", "keyfile", "launchmon", "none" }, SECURITY_DEFAULT_STR,
     "Set the security mode to the given value." },

   { confCmdlineNewgroup, "", shortNone, groupLauncher, cvBool, {}, "",
     "These options specify the job launcher Spindle is being run with.  If unspecified, Spindle will try to autodetect." },
   { confCmdlineOnly, "launcher-startup", shortLauncherStartup, groupLauncher, cvBool, {}, "false",
     "Launch spindle daemons using the system's job launcher (requires an already set-up session) (Deprecated)." },
   { confCmdlineOnly, "no-mpi", shortNoMPI, groupLauncher, cvBool, {}, "no",
     "Run serial job instead of a MPI job. (Deprecated for launcher=serial)" },
   { confCmdlineOnly, "serial", shortSerial, groupLauncher, cvBool, {}, "no",
     "Run serial job instead of a MPI job. Alias for --no-mpi. (Deprecated for launcher=serial)" },
   { confCmdlineOnly, "enable-hostbin", shortHostbinEnable, groupLauncher, cvBool, {}, HOSTBIN_ENABLE_STR,
     "Enables hostbin startup mode. (Derecated for launcher=hostbin)" },
   { confHostbin, "hostbin", shortHostbin, groupLauncher, cvString, {}, HOSTBIN_PATH_STR,
     "Path to a script that returns the hostlist for a job on a cluster." },   
   { confJoblauncher, "launcher", shortLauncher, groupLauncher, cvEnum, { "slurm", "flux", "flux-plugin", "slurm-plugin", "serial", "hostbin", "unknown" }, DEFAULT_LAUNCHER_STR,
     "Sets the launcher to the specified value." },

   { confCmdlineNewgroup, "", shortNone, groupSession, cvBool, {}, "",
     "Options for managing sessions, which can run multiple jobs out of one spindle cache." },
   { confStartSession, "start-session", shortStartSession, groupSession, cvBool, {}, "",
     "Start a persistent Spindle session." },
   { confStartMultiSession, "start-multi-session", shortStartMultiSession, groupSession, cvBool, {}, "",
     "Start a persistent Spindle session and print the session-id to stdout." },
   { confEndSession, "end-session", shortEndSession, groupSession, cvStringOptional, {}, "",
     "End a persistent Spindle session with the given session-id." },
   { confRunSession, "run-in-session", shortRunSession, groupSession, cvString, {}, "",
     "Run a new job in the given session." },

   { confCmdlineNewgroup, "", shortNone, groupNuma, cvBool, {}, "",
     "Options for controlling spindle's numa-centric optimizations, where it performs memory-aware library replication and placement." },
   { confNuma, "numa", shortNUMA, groupNuma, cvBool, {}, "false",
     "Enables NUMA-centric optimizations." },
   { confNumaIncludes, "numa-includes", shortNUMAIncludes, groupNuma, cvList, {}, "",
     "Colon-seperated list of substrings that will be matched to executables/libraries. Matches will have their memory replicated into each NUMA domain. "
     "If numa optimizations are enabled, but this option is not present, then all executables/libraries will have NUMA optimizations." },
   { confNumaExcludes, "numa-excludes", shortNUMAExcludes, groupNuma, cvList, {}, "",
     "Colon-seprated list of prefixes that will excludes executables/libraries from NUMA optimization. Takes precedence over numa includes." },

   { confCmdlineNewgroup, "", shortNone, groupMisc, cvBool, {}, "",
     "Misc options" },
   { confAuditType, "audit-type", shortAuditType, groupMisc, cvEnum, { "audit", "subaudit" }, AUDITTYPE_STR, 
     "Runs spindle in audit or subaudit mode. Subaudit is needed for certain glibc versions on PPC systems." },
   { confShmcacheSize, "shmcache-size", shortSharedCacheSize, groupMisc, cvInteger, {}, SHMCACHE_SIZE_STR,
     "Size of client shared memory cache in kb, which can be used to improve performance if multiple processes are running on each node." },
   { confPythonPrefix, "python-prefix", shortPythonPrefix, groupMisc, cvList, {}, PYTHON_PREFIX_DEFAULT,
     "Colon-seperated list of directories that contain the python install locations." },
   { confCachePrefix, "cache-prefix", shortCachePrefix, groupMisc, cvList, {}, "",
     "Alias for python-prefix" },
   { confExecExcludes, "exec-excludes", shortExecExcludes, groupMisc, cvList, {}, DEFAULT_EXEC_EXCLUDE_LIST,
     "Colon-seperated list of executable names that should not be run under spindle." },
   { confLocalPrefix, "local-prefix", shortLocalPrefix, groupMisc, cvList, {}, SPINDLE_LOCAL_PREFIX_STR,
     "Colon-seperated list of directories that spindle will not cache files out of" },
   { confDebug, "debug", shortDebug, groupMisc, cvBool, {}, "false",
     "If yes, hide spindle from debuggers so they think libraries come from the original locations.  May cause extra overhead." },
   { confPatchLdso, "patch-ldso", shortPatchLdso, groupMisc, cvBool, {}, DEFAULT_PATCH_LDSO,
     "Enables or disables dynamic binary patching of ld.so to better intercept stat calls." },
   { confPreload, "preload", shortPreload, groupMisc, cvString, {}, "",
     "Provides a text file containing a white-space separated list of files that should be relocated to each node before execution begins" },
   { confStrip, "strip", shortStrip, groupMisc, cvBool, {}, "true", 
     "Strip debug and symbol information from binaries before distributing them." },
   { confCommPath, "commpath", shortCommPath, groupMisc, cvString, {}, SPINDLE_COMMPATH_STR,
     "Back-end directory communication and housekeeping.  Should be a non-shared location such as a ramdisk." },
   { confCachePaths, "cachepaths", shortCachePaths, groupMisc, cvString, {}, SPINDLE_CACHEPATHS_STR,
     "Colon-separated list of candidate paths for cached libraries."},
   { confNoclean, "noclean", shortNoClean, groupMisc, cvBool, {}, "false",
     "Don't remove local file cache after execution." },
   { confDisableLogging, "disable-logging", shortDisableLogging, groupMisc, cvBool, {}, DISABLE_LOGGING_STR,
     "Disable usage logging for this invocation of Spindle." },
   { confNoHide, "no-hide", shortHide, groupMisc, cvBool, {}, "false", 
     "Don't hide spindle file descriptors from application." },
   { confPersist, "persist", shortPersist, groupMisc, cvBool, {}, "false",
     "Allow spindle servers to persist after the last client job has exited." },
   { confMsgcacheBuffer, "msgcache-buffer", shortMsgcacheBuffer, groupMisc, cvInteger, {}, "0",
     "Enables message buffering if size is non-zero, and sets the size of the buffer in kilobytes." },
   { confMsgcacheTimeout, "msgcache-timeout", shortMsgcacheTimeout, groupMisc, cvInteger, {}, "0",
     "Enables message buffering if size is non-zero, and sets the buffering timeout in milliseconds." },
   { confCleanupProc, "cleanup-proc", shortCleanupProc, groupMisc, cvBool, {}, CLEANUP_PROC_STR,
     "Fork a dedicated process to clean-up files post-spindle.  Useful for high-fault situations." },
   { confEnableRsh, "enable-rsh", shortRSHMode, groupMisc, cvBool, {}, RSHLAUNCH_STR,
     "Enable starting daemons with an rsh tree, if the startup mode supports it." },
   { confRshCommand, "rsh-command", shortRSHCmd, groupMisc, cvString, {}, RSHCMD_STR,
     "The command to run rsh/ssh, when doing RSH startup mode." }
  } );
}

pair<bool, bool> strToBool(string s)
{
   if (strcasecmp(s.c_str(), "true") == 0)
      return make_pair(true, true);
   if (strcasecmp(s.c_str(), "yes") == 0)
      return make_pair(true, true);
   if (strcasecmp(s.c_str(), "1") == 0)
      return make_pair(true, true);
   if (strcasecmp(s.c_str(), "false") == 0)
      return make_pair(true, false);
   if (strcasecmp(s.c_str(), "no") == 0)
      return make_pair(true, false);
   if (strcasecmp(s.c_str(), "0") == 0)
      return make_pair(true, false);
   return make_pair(false, false);
}

static pair<bool, long> strToLong(string s)
{
   size_t pos = 0;
   long l = 0;
   try {
      l = std::stol(s, &pos, 0);
   } catch (std::invalid_argument &e) {
      pos = 0;
   }
   if (!pos || s.length() != pos) {
      return make_pair(false, 0);
   }
   return make_pair(true, l);
}

static pair<bool, string> strToEnum(SpindleConfigID name, string value) {
   map<SpindleConfigID, const SpindleOption&>::const_iterator i = getOptionsByID().find(name);
   if (i == getOptionsByID().end()) {
      err_printf("Unknown SpindleConfigID %d\n", (int) name);
      return make_pair(false, string());
   }
   const SpindleOption &opt = i->second;
   const list<const char *> &enum_values = opt.enum_values;
   
   for (list<const char *>::const_iterator j = enum_values.begin(); j != enum_values.end(); j++) {
      const char *s = *j;
      if (strcasecmp(s, value.c_str()) == 0) {
         return make_pair(true, value);
      }
   }
   debug_printf("Config parsing error: '%s' does not have an enum value '%s'\n",
                opt.cmdline_long, value.c_str());
   return make_pair(false, string());
}

ConfigMap::ConfigMap(string origin_) :
   origin(origin_),
   unique_id_saved(0),
   number_saved(0)
{
}

ConfigMap::ConfigMap() :
   origin(),
   unique_id_saved(0),
   number_saved(0)
{
}

void ConfigMap::setOrigin(string s) {
   origin = s;
}

void ConfigMap::setApplicationCmdline(int argc, char **argv)
{
   for (int i = 0; i < argc; i++) {
      app_commandline.push_back(string(argv[i]));
   }
}

#define LIST_RESET_TOKEN "NONE"
#define LIST_RESET_TOKEN_SIZE 4
string ConfigMap::mergeLists(string value, string existing)
{
   std::size_t pos = value.find(LIST_RESET_TOKEN);
   if (pos == string::npos) {
      return existing.empty() ? value : value + ":" + existing; 
   }
   std::size_t next_term_start = pos + LIST_RESET_TOKEN_SIZE;
   bool none_starts_term = (pos == 0) || (value[pos-1] == ':');
   bool none_ends_term = (next_term_start == value.length()) || (value[next_term_start] == ':');
   if (!none_starts_term || !none_ends_term) {
      return existing.empty() ? value : value + ":" + existing; 
   }

   if (next_term_start == value.length())
      return std::string();
   else
      return value.substr(next_term_start + 1);
}

bool ConfigMap::mergeOnto(const ConfigMap &other)
{
   for (map<SpindleConfigID, string>::const_iterator i = other.name_values.begin(); i != other.name_values.end(); i++) {
      SpindleConfigID name = i->first;
      string value = i->second;
      map<SpindleConfigID, const SpindleOption&>::const_iterator j = getOptionsByID().find(name);
      if (j == getOptionsByID().end()) {
         err_printf("Unknown option %d\n", (int) name);
         return false;
      }
      const SpindleOption& opt = j->second;
      ConfigValueType conftype = opt.conftype;

      map<SpindleConfigID, string>::const_iterator existing_valuei = name_values.find(name);
      string existing_value = existing_valuei != name_values.end() ? existing_valuei->second : "";
      
      string newvalue;
      if (conftype == cvList) {
         newvalue = mergeLists(value, existing_value);
         debug_printf3("Config parsing merging lists for %s '%s' and '%s' from configmap %s to '%s'\n",
                       opt.cmdline_long, existing_value.c_str(), value.c_str(), other.origin.c_str(), newvalue.c_str());
      }
      else if (!existing_value.empty() && existing_value != value) {
         debug_printf3("Config parsing overwriting existing value for %s '%s' with '%s' from %s\n",
                       opt.cmdline_long, existing_value.c_str(), value.c_str(), other.origin.c_str());
         newvalue = value;
      }
      else {
         newvalue = value;
      }
      
      name_values[name] = newvalue;
   }
   if (!other.app_commandline.empty()) {
      app_commandline = other.app_commandline;
   }
   if (other.unique_id_saved)
      unique_id_saved = other.unique_id_saved;
   if (other.number_saved)
      number_saved = other.number_saved;
   return true;
}

bool ConfigMap::set(SpindleConfigID name, string value, string &errstring) {
   errstring = string();
   map<SpindleConfigID, const SpindleOption&>::const_iterator j = getOptionsByID().find(name);
   if (j == getOptionsByID().end()) {
      err_printf("Unknown option %d\n", (int) name);
      return false;
   }
   const SpindleOption& opt = j->second;
   ConfigValueType conftype = opt.conftype;

   if (conftype == cvBool && !strToBool(value).first) {
      stringstream ss;
      ss << "Expected bool type for " << opt.cmdline_long << ". Got '" << value << "'.\n";
      errstring = ss.str();
      return false;      
   }
   else if (conftype == cvInteger && !strToLong(value).first) {
      stringstream ss;
      ss << "Expected bool type for " << opt.cmdline_long << ". Got '" << value << "'.\n";
      errstring = ss.str();
      return false;
   }
   else if (conftype == cvEnum && !strToEnum(name, value).first) {
      stringstream ss;
      ss << "Unexpected enum value for " << opt.cmdline_long << ". Got '" << value << "'. Expected: ";
      const list<const char *> &expected = opt.enum_values;
      bool first = true;
      for (list<const char *>::const_iterator j = expected.begin(); j != expected.end(); j++) {
         if (!first)
            ss << ", ";
         ss << "'" << *j << "'";
         first = false;
      }
      ss << "\n";
      errstring = ss.str();
      return false;
   }
   
   name_values[name] = value;
   return true;
}

void ConfigMap::debugPrint() const {
   debug_printf("Configuration:\n");
   map<SpindleConfigID, const SpindleOption&> optsbyid = getOptionsByID();
   for (map<SpindleConfigID, const SpindleOption&>::iterator i = optsbyid.begin(); i != optsbyid.end(); i++) {
      SpindleConfigID name = i->first;
      const SpindleOption& opt = i->second;
      
      map<SpindleConfigID, string>::const_iterator j = name_values.find(name);
      if (j == name_values.end()) {
         debug_printf("CONFIG: %s = [UNSET]\n", opt.cmdline_long);
         continue;
      }

      string values = j->second;
      bool typeerror = false;
      switch (opt.conftype) {
         case cvBool: {
            pair<bool, bool> result = strToBool(values);
            typeerror = !result.first;
            if (!typeerror) {
               debug_printf("CONFIG: %s = %s\n", opt.cmdline_long, result.second ? "true" : "false");
               continue;
            }
            break;
         }
         case cvInteger: {
            pair<bool, long> result = strToLong(values);
            typeerror = !result.first;
            if (!typeerror) {
               debug_printf("CONFIG: %s = %ld\n", opt.cmdline_long, result.second);
               continue;
            }
            break;
         }
         case cvString:
         case cvList:
            debug_printf("CONFIG: %s = %s\n", opt.cmdline_long, values.c_str());
            continue;
         case cvStringOptional:
            debug_printf("CONFIG: %s = %s\n", opt.cmdline_long, values.empty() ? "[PRESENT]" : values.c_str());
            continue;
         case cvEnum:
            pair<bool, string> result = strToEnum(name, values);
            typeerror = !result.first;
            if (!typeerror) {
               debug_printf("CONFIG: %s = %s\n", opt.cmdline_long, result.second.c_str());
               continue;
            }
            break;
      }

      if (typeerror) 
         err_printf("CONFIG: %s = [TYPE ERROR]\n", opt.cmdline_long);
      else
         err_printf("CONFIG: %s = [INTERNAL ERROR]\n", opt.cmdline_long);
   }
}

pair<bool, string> ConfigMap::getValueString(SpindleConfigID name) const {
   map<SpindleConfigID, string>::const_iterator i = name_values.find(name);
   if (i == name_values.end()) {
      return make_pair(false, "");
   }
   return make_pair(true, i->second);
}

pair<bool, string> ConfigMap::getValueEnum(SpindleConfigID name) const {
   map<SpindleConfigID, string>::const_iterator i = name_values.find(name);
   if (i == name_values.end()) {
      return make_pair(false, "");
   }
   return strToEnum(name, i->second);
}

pair<bool, long> ConfigMap::getValueIntegral(SpindleConfigID name) const {
   map<SpindleConfigID, string>::const_iterator i = name_values.find(name);
   if (i == name_values.end()) {
      return make_pair(false, 0);
   }
   return strToLong(i->second);
}

pair<bool, bool> ConfigMap::getValueBool(SpindleConfigID name) const {
   map<SpindleConfigID, string>::const_iterator i = name_values.find(name);
   if (i == name_values.end()) {
      return make_pair(false, false);
   }
   return strToBool(i->second);
}

bool ConfigMap::getApplicationCmdline(int &argc, char** &argv) const {
   if (app_commandline.empty()) {
      err_printf("Asked for application cmdline, but cmdline is empty\n");
      return false;
   }
   argc = app_commandline.size();
   argv = (char **) malloc((argc + 1) * sizeof(char*));
   int j = 0;
   for (list<string>::const_iterator i = app_commandline.begin(); i != app_commandline.end(); i++, j++) {
      argv[j] = strdup(i->c_str());
   }
   argv[j] = NULL;
   return true;
}

bool ConfigMap::isSet(SpindleConfigID name) const {
   map<SpindleConfigID, string>::const_iterator i = name_values.find(name);
   return i != name_values.end();
}

bool getDefaultsConfigMap(ConfigMap &confmap, string &errstring) {
   initOptionsList();
   for (list<SpindleOption>::const_iterator i = Options->begin(); i != Options->end(); i++) {
      const SpindleOption &opt = *i;
      if (opt.config_id == confCmdlineOnly || opt.config_id == confCmdlineNewgroup)
         continue;
      if (!opt.default_value || opt.default_value[0] == '\0')
         continue;
      bool result = confmap.set(opt.config_id, string(opt.default_value), errstring);
      if (!result) {
         err_printf("Error setting default value of %s: %s\n", opt.cmdline_long, errstring.c_str());
         return false;
      }
   }
   return true;
}

bool gatherAllConfigInfo(int argc, char *argv[], bool inExecutable, ConfigMap &config, string &errmessage) {
   initOptionsList();
   ConfigMap defaults("[DEFAULTS]");
   bool result;

   string errmsg;
   result = getDefaultsConfigMap(defaults, errmsg);
   if (!result) {
      debug_printf("getDefaultConfigs returned error. Aborting config setup\n");
      errmessage = "Error processing compiled-in configs (Spindle may be mis-built): " + errmsg;
      return false;
   }
   config.mergeOnto(defaults);

   ConfigFile system_spindle_conf(PKGSYSCONFDIR "/spindle.conf");
   if (system_spindle_conf.openError()) {
      debug_printf("Skipping reading system config: %s\n", system_spindle_conf.errorString().c_str());
   }
   else if (system_spindle_conf.parseError()) {
      errmessage = system_spindle_conf.errorString();
      debug_printf("Aborting after parsing system config: %s\n", errmessage.c_str());
      return false;
   }
   else {
      debug_printf2("Merging system config into global config\n");
      config.mergeOnto(system_spindle_conf.getConfigMap());
   }

   const char *user_home = getenv("HOME");
   string user_config_file = user_home ? (string(user_home) + "/.spindle/spindle.conf") : "";
   ConfigFile user_spindle_conf(user_config_file);
   if (user_spindle_conf.openError()) {
      debug_printf("Skipping reading user config: %s\n", user_spindle_conf.errorString().c_str());
   }
   else if (user_spindle_conf.parseError()) {
      errmessage = user_spindle_conf.errorString();
      debug_printf("Aborting after parsing user config: %s\n", errmessage.c_str());
      return false;
   }
   else {
      debug_printf2("Merging user config into global config\n");
      config.mergeOnto(user_spindle_conf.getConfigMap());
   }

   ConfigMap cmdline_config("[CMDLINE]");
   int argresult = parseArgs(argc, argv, inExecutable, errmessage, cmdline_config);
   if (argresult == -1) {
      debug_printf("Failed to parse command line: %s\n", errmessage.c_str());
      return false;
   }
   config.mergeOnto(cmdline_config);

   config.debugPrint();
   return true;
}

static void setopt(opt_t &optv, opt_t opttype, bool setv) {
   if (setv)
      optv |= opttype;
   else
      optv &= ~opttype;
}

static char *getstr(const string& value, bool alloc_strs) {
   if (!alloc_strs) {
      return const_cast<char*>(value.c_str());
   }
   else {
      return strdup(value.c_str());
   }
}

bool ConfigMap::toSpindleArgs(spindle_args_t &args, bool alloc_strs) const
{
   for (map<SpindleConfigID, string>::const_iterator i = name_values.begin(); i != name_values.end(); i++) {
      SpindleConfigID name = i->first;
      const string& value = i->second;
      
      map<SpindleConfigID, const SpindleOption &>::const_iterator j = getOptionsByID().find(name);
      if (j == getOptionsByID().end()) {
         err_printf("Unknown option id %d\n", (int) name);
         return false;
      }
      const SpindleOption &opt = j->second;

      bool boolresult = false;
      const string &strresult = value;
      long numresult = 0;
      
      switch (opt.conftype) {
         case cvBool: {
            pair<bool, bool> result = getValueBool(name);
            if (!result.first) {
               err_printf("Error accessing option %s\n", opt.cmdline_long);
               return false;
            }
            boolresult = result.second;
            break;
         }
         case cvInteger: {
            pair<bool, long> result = getValueIntegral(name);
            if (!result.first) {
               err_printf("Error accessing option %s\n", opt.cmdline_long);
               return false;
            }
            numresult = result.second;
            break;
         }
         case cvString:
         case cvList:
            //strresult already set to value
            break;
         case cvStringOptional:
            break;
         case cvEnum: {
            pair<bool, string> result = getValueEnum(name);
            if (!result.first) {
               err_printf("Error accessing option %s\n", opt.cmdline_long);
               return false;
            }
            //strresult already set to value
         }
      }

      switch (name) {
         case confCmdlineOnly:
            break;
         case confCmdlineNewgroup:
            break;
         case confPort:
            args.port = numresult;
            break;
         case confNumPorts:
            args.num_ports = numresult;
            break;
         case confCommPath: {
            string path = strresult + "/spindle.$NUMBER";
            args.commpath = strdup(path.c_str());
            break;
         }
         case confCachePaths:{
            // Paramemter values are colon-separated lists of paths.
            // Append "/spindle.$NUMBER" to each path in the list.
            string paths = strresult;
            size_t idx = paths.find(":");
            string number_var_with_colon("/spindle.$NUMBER:");
            string number_var_without_colon("/spindle.$NUMBER");
            while( idx != string::npos ){
               paths.replace(idx, 1, number_var_with_colon);
               idx = paths.find(":", idx + number_var_with_colon.size());
            };
            paths += number_var_without_colon;
            args.candidate_cachepaths = strdup(paths.c_str());
            break;
         }
         case confCachePrefix:
         case confPythonPrefix:
            if (args.pythonprefix)
               args.pythonprefix = getstr(string(args.pythonprefix) + string(":") + strresult, true);
            else
               args.pythonprefix = getstr(strresult, alloc_strs);
            break;
         case confExecExcludes:
            args.exec_excludes = getstr(strresult, alloc_strs);
            break;
         case confLocalPrefix:
            args.local_prefixes = getstr(strresult, alloc_strs);
            break;
         case confStrip:
            setopt(args.opts, OPT_STRIP, boolresult);
            break;
         case confRelocAout:
            setopt(args.opts, OPT_RELOCAOUT, boolresult);
            break;
         case confRelocLibs:
            setopt(args.opts, OPT_RELOCSO, boolresult);
            break;
         case confRelocPython:
            setopt(args.opts, OPT_RELOCPY, boolresult);
            break;
         case confRelocExec:
            setopt(args.opts, OPT_RELOCEXEC, boolresult);
            break;
         case confFollowFork:
            setopt(args.opts, OPT_FOLLOWFORK, boolresult);
            break;
         case confStopReloc:
            setopt(args.opts, OPT_STOPRELOC, boolresult);
            break;
         case confSpindleLevel:
            if (strresult == "default") {
               setopt(args.opts, OPT_OFF, false);              
            }
            if (strresult == "high") {
               setopt(args.opts, OPT_RELOCAOUT, true);
               setopt(args.opts, OPT_RELOCSO, true);
               setopt(args.opts, OPT_RELOCPY, true);
               setopt(args.opts, OPT_RELOCEXEC, true);
               setopt(args.opts, OPT_FOLLOWFORK, true);
               setopt(args.opts, OPT_STOPRELOC, false);
               setopt(args.opts, OPT_OFF, false);
            }
            if (strresult == "medium") {
               setopt(args.opts, OPT_RELOCAOUT, false);
               setopt(args.opts, OPT_RELOCSO, true);
               setopt(args.opts, OPT_RELOCPY, true);
               setopt(args.opts, OPT_RELOCEXEC, false);
               setopt(args.opts, OPT_FOLLOWFORK, true);
               setopt(args.opts, OPT_STOPRELOC, false);
               setopt(args.opts, OPT_OFF, false);
            }
            if (strresult == "low") {
               setopt(args.opts, OPT_RELOCAOUT, false);
               setopt(args.opts, OPT_RELOCSO, false);
               setopt(args.opts, OPT_RELOCPY, false);
               setopt(args.opts, OPT_RELOCEXEC, false);
               setopt(args.opts, OPT_FOLLOWFORK, true);
               setopt(args.opts, OPT_STOPRELOC, true);
               setopt(args.opts, OPT_OFF, false);
            }
            if (strresult == "off") {
               setopt(args.opts, OPT_RELOCAOUT, false);
               setopt(args.opts, OPT_RELOCSO, false);
               setopt(args.opts, OPT_RELOCPY, false);
               setopt(args.opts, OPT_RELOCEXEC, false);
               setopt(args.opts, OPT_FOLLOWFORK, false);
               setopt(args.opts, OPT_STOPRELOC, false);
               setopt(args.opts, OPT_OFF, true);
            }
            break;
         case confPushpull:
            if (strresult == "pull") {
               setopt(args.opts, OPT_PULL, true);
               setopt(args.opts, OPT_PUSH, false);
            }
            else {
               setopt(args.opts, OPT_PUSH, true);
               setopt(args.opts, OPT_PULL, false);
            }
            break;
         case confNetwork:
            if (strresult == "cobo")
               setopt(args.opts, OPT_COBO, true);
            break;
         case confSecurity:
            if (strresult == "munge") {
               OPT_SET_SEC(args.opts, OPT_SEC_MUNGE);
            }
            else if (strresult == "keyfile") {
               OPT_SET_SEC(args.opts, OPT_SEC_KEYFILE);
            }
            else if (strresult == "launchmon") {
               OPT_SET_SEC(args.opts, OPT_SEC_KEYLMON);               
            }
            else if (strresult == "none") {
               OPT_SET_SEC(args.opts, OPT_SEC_NULL);               
            }
            else {
               err_printf("Unknown security mode: %s\n", strresult.c_str());
               return false;
            }
            break;
         case confJoblauncher:
            if (strresult == "slurm") {
               debug_printf("Setting srun_launcher as job launcher\n");
               args.startup_type = startup_mpi;
               args.use_launcher = srun_launcher;
               debug_printf("launcher = %u\n", args.startup_type);
            }
            else if (strresult == "flux-plugin") {
               debug_printf("Setting flux_plugin_launcher as job launcher\n");
               args.startup_type = startup_external;
               args.use_launcher = flux_plugin_launcher;
            }
            else if (strresult == "flux") {
               debug_printf("Setting flux_session_launcher as job launcher\n");
               args.startup_type = startup_mpi;
               args.use_launcher = flux_session_launcher;
            }
            else if (strresult == "slurm-plugin") {
               debug_printf("Setting slurm_plugin_launcher as job launcher\n");
               args.startup_type = startup_external;
               args.use_launcher = slurm_plugin_launcher;
            }
            else if (strresult == "serial") {
               debug_printf("Setting serial as job launcher\n");
               args.startup_type = startup_serial;
               args.use_launcher = serial_launcher;
            }
            else if (strresult == "hostbin") {
               debug_printf("Setting hostbin as job launcher\n");
               args.startup_type = startup_hostbin;
               args.use_launcher = marker_launcher;
            }
            else if (strresult == "unknown") {
               debug_printf("Job launcher is unknown\n");
            }
            else {
               err_printf("Unknown job launcher: %s\n", strresult.c_str());
               return false;
            }
            break;
         case confNuma:
            setopt(args.opts, OPT_NUMA, boolresult);
            break;
         case confNumaIncludes:
            setopt(args.opts, OPT_NUMA, true);
            args.numa_files = getstr(strresult, alloc_strs);
            break;
         case confNumaExcludes:
            args.numa_excludes = getstr(strresult, alloc_strs);
            break;
         case confAuditType:
            if (strresult == "audit") {
               setopt(args.opts, OPT_SUBAUDIT, false);
            }
            else if (strresult == "subaudit") {
               setopt(args.opts, OPT_SUBAUDIT, true);
            }
            else {
               err_printf("Unknown audit type %s\n", strresult.c_str());
               return false;
            }
            break;
         case confShmcacheSize:
            args.shm_cache_size = (unsigned int) numresult;
            setopt(args.opts, OPT_SHMCACHE, (numresult != 0));
            break;
         case confDebug:
            setopt(args.opts, OPT_DEBUG, boolresult);
            break;
         case confPreload:
            args.preloadfile = getstr(strresult, alloc_strs);
            break;
         case confHostbin:
            if (!strresult.empty()) {
               args.startup_type = startup_hostbin;
               args.use_launcher = marker_launcher;
            }               
            break;
         case confNoclean:
            setopt(args.opts, OPT_NOCLEAN, boolresult);
            break;
         case confDisableLogging:
            setopt(args.opts, OPT_LOGUSAGE, !boolresult);
            break;
         case confNoHide:
            setopt(args.opts, OPT_NOHIDE, boolresult);
            break;
         case confPersist:
            setopt(args.opts, OPT_PERSIST, boolresult);
            break;
         case confMsgcacheBuffer:
            if (numresult) {
               setopt(args.opts, OPT_MSGBUNDLE, true);
               args.bundle_cachesize_kb = (unsigned int) numresult;
            }
            else {
               setopt(args.opts, OPT_MSGBUNDLE, false);
            }
            break;
         case confMsgcacheTimeout:
            if (numresult) {
               setopt(args.opts, OPT_MSGBUNDLE, true);
               args.bundle_timeout_ms = (unsigned int) numresult;
            }
            else {
               setopt(args.opts, OPT_MSGBUNDLE, false);
            }
            break;
         case confCleanupProc:
            setopt(args.opts, OPT_PROCCLEAN, boolresult);
            break;
         case confEnableRsh:
            setopt(args.opts, OPT_RSHLAUNCH, boolresult);
            break;
         case confRshCommand:
            args.rsh_command = getstr(strresult, alloc_strs);
            break;
         case confStartSession:
         case confStartMultiSession:
            setopt(args.opts, OPT_SESSION, boolresult);
            break;
         case confEndSession:
         case confRunSession:
            setopt(args.opts, OPT_SESSION, !strresult.empty());            
            break;
         case confPatchLdso:
            setopt(args.opts, OPT_PATCHLDSO, boolresult);
            break;
      }
   }

   /* Set any option dependencies */
   if (args.startup_type == startup_serial) {
      setopt(args.opts, OPT_RSHLAUNCH, false);
   }
   if (args.opts & OPT_SESSION) {
      args.opts |= OPT_PERSIST;
   }
   if (args.opts & OPT_DEBUG) {
      args.opts |= OPT_REMAPEXEC;
   }
   return true;
}

session_status_t ConfigMap::getSessionStatus() const
{
   if (isSet(confStartSession))
      return sstatus_start;
   else if (isSet(confStartMultiSession))
      return sstatus_start;
   else if (isSet(confEndSession))
      return sstatus_end;
   else if (isSet(confRunSession))
      return sstatus_run;
   else
      return sstatus_unused;
}

std::string ConfigMap::getArgSessionId() const
{
   SpindleConfigID conf;
   if (isSet(confRunSession))
      conf = confRunSession;
   else if (isSet(confEndSession))
      conf = confEndSession;
   else
      assert(0);
   pair<bool, string> result = getValueString(conf);
   return result.second;
}

static unsigned int str_hash(const char *str)
{
   unsigned long hash = 5381;
   int c;
   while ((c = *str++))
      hash = ((hash << 5) + hash) + c;
   return (unsigned int) hash;
}

bool getRandom(void *bytes, size_t bytes_size)
{
   int fd = open("/dev/urandom", O_RDONLY);
   if (fd == -1)
      fd = open("/dev/random", O_RDONLY);
   if (fd == -1) {
      int error = errno;
      err_printf("Could not open /dev/urandom or /dev/random: %s\n", strerror(error));
      return false;
   }

   int retries = 10;
   size_t bytes_read = 0;
   unsigned char *bytes_c = (unsigned char *) bytes;
   do {
      ssize_t result = read(fd, bytes_c + bytes_read, bytes_size - bytes_read);
      if (result == -1 && errno == EINTR) {
         continue;
      }
      if (result == -1 && errno == EAGAIN) {
         if (retries-- > 0)
            continue;         
      }
      if (result == -1) {
         int error = errno;
         err_printf("Could not read from /dev/urandom or /dev/random: %s\n", strerror(error));
         close(fd);
         return false;
      }
      bytes_read += (size_t) result;
   } while (bytes_read < bytes_size);
   close(fd);
   return true;
}

number_t ConfigMap::getNumber() const
{
   if (number_saved)
      return number_saved;
   getRandom(&number_saved, sizeof(number_saved));
   return number_saved;
}

bool ConfigMap::getUniqueID(unique_id_t &unique_id, string &errmsg) const
{
   /* This needs only needs to be unique between overlapping Spindle instances
      actively running on the same node.  Grab 16-bit pid, 16-bits hostname hash,
      32-bits randomness */
   if (unique_id_saved) {
      unique_id = unique_id_saved;
      return true;
   }
   uint16_t pid = (uint16_t) getpid();
   
   char hostname[256];
   gethostname(hostname, sizeof(hostname));
   hostname[sizeof(hostname)-1] = '\0';
   uint16_t hostname_hash = (uint16_t) str_hash(hostname);

   uint32_t random;
   bool result = getRandom(&random, sizeof(random));
   if (!result) {
      errmsg = "Could not read random bytes from /dev/random or /dev/urandom";
      return false;
   }

   unique_id = (uint16_t) pid;
   unique_id |= ((uint64_t) hostname_hash) << 16;
   unique_id |= ((uint64_t) random) << 32;

   unique_id_saved = unique_id;
   return true;
}

const map<SpindleConfigID, const SpindleOption&> &getOptionsByID()
{
   static map<SpindleConfigID, const SpindleOption&> optsbyid;
   if (optsbyid.empty()) {
      for (list<SpindleOption>::const_iterator i = Options->begin(); i != Options->end(); i++) {
         const SpindleOption &opt = *i;
         if (opt.config_id == confCmdlineOnly || opt.config_id == confCmdlineNewgroup)
            continue;
         optsbyid.insert(make_pair(opt.config_id, std::ref(opt)));
      }
   }
   return optsbyid;
}

const map<CmdlineShortOptions, const SpindleOption&> &getOptionsByKey()
{
   static map<CmdlineShortOptions, const SpindleOption&> optsbyshort;
   if (optsbyshort.empty()) {
      for (list<SpindleOption>::const_iterator i = Options->begin(); i != Options->end(); i++) {
         const SpindleOption &opt = *i;
         if (opt.cmdline_key == shortNone)
            continue;
         optsbyshort.insert(make_pair(opt.cmdline_key, std::ref(opt)));
      }
   }
   return optsbyshort;
}

const map<string, const SpindleOption&> &getOptionsByLongName()
{
   static map<string, const SpindleOption&> optsbylong;
   if (optsbylong.empty()) {
      for (list<SpindleOption>::const_iterator i = Options->begin(); i != Options->end(); i++) {
         const SpindleOption &opt = *i;
         if (!opt.cmdline_long || opt.cmdline_long[0] == '\0')
            continue;
         optsbylong.insert(make_pair(string(opt.cmdline_long), std::ref(opt)));
      }
   }
   return optsbylong;
}
