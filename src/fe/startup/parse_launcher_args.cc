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

#include "parse_launcher.h"
#include <cassert>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cstring>

using namespace std;

static const unsigned int serial_size = 1;
static cmdoption_t serial_options[] = {
   { NULL, NULL, FL_LAUNCHER }
};
static const char *serial_bg_env_str = "";

static cmdoption_t openmpi_options[] = {
   { "mpiexec", NULL,                FL_LAUNCHER },
   { "mpirun", NULL,                 FL_LAUNCHER },
   { "orterun", NULL,                FL_LAUNCHER },
   { "-am", NULL,                    FL_PARAM },
   { "--app", NULL,                  FL_CUSTOM_OPTION },
   { "-bind-to-board", NULL,         FL_OPTIONAL_DASH },
   { "-bind-to-core", NULL,          FL_OPTIONAL_DASH },
   { "-bind-to-none", NULL,          FL_OPTIONAL_DASH },
   { "-bind-to-socket", NULL,        FL_OPTIONAL_DASH },
   { "-byboard", NULL,               FL_OPTIONAL_DASH },
   { "-bycore", NULL,                FL_OPTIONAL_DASH },
   { "-bynode", NULL,                FL_OPTIONAL_DASH },
   { "-byslot", NULL,                FL_OPTIONAL_DASH },
   { "-bysocket", NULL,              FL_OPTIONAL_DASH },
   { "-np", NULL,                    FL_OPTIONAL_DASH | FL_PARAM },
   { "-c", NULL,                     FL_PARAM },
   { "-cf", NULL,                    FL_PARAM },
   { "--cartofile", NULL,            FL_PARAM },
   { "-cpus-per-proc", NULL,         FL_OPTIONAL_DASH | FL_PARAM },
   { "-cpus-per-rank", NULL,         FL_OPTIONAL_DASH | FL_PARAM },
   { "-debug-devel", NULL,           FL_OPTIONAL_DASH },
   { "-d", NULL,                     0 },
   { "-debug", NULL,                 FL_OPTIONAL_DASH },
   { "-debug-daemons", NULL,         FL_OPTIONAL_DASH },
   { "-debug-daemons-file", NULL,    FL_OPTIONAL_DASH },
   { "-debugger", NULL,              FL_OPTIONAL_DASH | FL_PARAM },
   { "-default-hostfile", NULL,      FL_OPTIONAL_DASH | FL_PARAM },
   { "-display-allocation", NULL,    FL_OPTIONAL_DASH },
   { "-display-devel-allocation", NULL, FL_OPTIONAL_DASH },
   { "-display-devel-map", NULL,     FL_OPTIONAL_DASH },
   { "-display-map", NULL,           FL_OPTIONAL_DASH },
   { "-do-not-launch", NULL,         FL_OPTIONAL_DASH },
   { "-do-not-resolve", NULL,        FL_OPTIONAL_DASH },
   { "-gmca", NULL,                  FL_OPTIONAL_DASH | FL_PARAM2 },
   { "-h", NULL,                     FL_OPTIONAL_DASH },
   { "-host", NULL,                  FL_OPTIONAL_DASH | FL_PARAM },
   { "-H", NULL,                     FL_PARAM },
   { "--hetero", NULL,               0 },
   { "-hostfile", NULL,              FL_OPTIONAL_DASH | FL_PARAM },
   { "-launch-agent", NULL,          FL_OPTIONAL_DASH | FL_PARAM },
   { "-leave-session-attached", NULL,FL_OPTIONAL_DASH },
   { "-loadbalance", NULL,           FL_OPTIONAL_DASH },
   { "-machinefile", NULL,           FL_OPTIONAL_DASH | FL_PARAM },
   { "-mca", NULL,                   FL_OPTIONAL_DASH | FL_PARAM2 },
   { "-n", NULL,                     FL_OPTIONAL_DASH | FL_PARAM },
   { "-nolocal", NULL,               FL_OPTIONAL_DASH },
   { "-nooversubscribe", NULL,       FL_OPTIONAL_DASH },
   { "--noprefix", NULL,             0 },
   { "-nperboard", NULL,             FL_OPTIONAL_DASH | FL_PARAM },
   { "-npernode", NULL,              FL_OPTIONAL_DASH | FL_PARAM },
   { "-npersocket", NULL,            FL_OPTIONAL_DASH | FL_PARAM },
   { "-num-boards", NULL,            FL_OPTIONAL_DASH | FL_PARAM },
   { "-num-cores", NULL,             FL_OPTIONAL_DASH | FL_PARAM },
   { "-num-sockets", NULL,           FL_OPTIONAL_DASH | FL_PARAM },
   { "-ompi-server", NULL,           FL_OPTIONAL_DASH | FL_PARAM },
   { "-output-filename", NULL,       FL_OPTIONAL_DASH | FL_PARAM },
   { "-path", NULL,                  FL_OPTIONAL_DASH | FL_PARAM | FL_EXEDIR },
   { "-pernode", NULL,               FL_OPTIONAL_DASH },
   { "--prefix", NULL,               FL_PARAM },
   { "--preload-files", NULL,        FL_PARAM },
   { "--preload-files-dest-dir", NULL, FL_PARAM },
   { "-q", NULL,                     0 },
   { "--quiet", NULL,                0 },
   { "-report-bindings", NULL,       FL_OPTIONAL_DASH },
   { "-report-events", NULL,         FL_OPTIONAL_DASH | FL_PARAM },
   { "-report-pid", NULL,            FL_OPTIONAL_DASH | FL_PARAM },
   { "-report-uri", NULL,            FL_OPTIONAL_DASH | FL_PARAM },
   { "-rf", NULL,                    FL_PARAM },
   { "--rankfile", NULL,             FL_PARAM },
   { "-s", NULL,                     0 },
   { "--preload-binary", NULL,       0 },
   { "-server-wait-time", NULL,      FL_OPTIONAL_DASH | FL_PARAM },
   { "-show-progress", NULL,         FL_OPTIONAL_DASH },
   { "-slot-list", NULL,             FL_OPTIONAL_DASH | FL_PARAM },
   { "-stdin", NULL,                 FL_OPTIONAL_DASH | FL_PARAM },
   { "-stride", NULL,                FL_OPTIONAL_DASH | FL_PARAM },
   { "-tag-output", NULL,            FL_OPTIONAL_DASH },
   { "-timestamp-output", NULL,      FL_OPTIONAL_DASH },
   { "-tmpdir", NULL,                FL_OPTIONAL_DASH | FL_PARAM },
   { "-tv", NULL,                    FL_OPTIONAL_DASH },
   { "-use-regexp", NULL,            FL_OPTIONAL_DASH },
   { "-v", NULL,                     0 },
   { "--verbose", NULL,              0 },
   { "-V", NULL,                     0 },
   { "--version", NULL,              0 },
   { "-wait-for-server", NULL,       FL_OPTIONAL_DASH },
   { "-wd", NULL,                    FL_OPTIONAL_DASH | FL_PARAM | FL_EXEDIR },
   { "-wdir", NULL,                  FL_OPTIONAL_DASH | FL_PARAM | FL_EXEDIR },
   { "-x", NULL,                     FL_PARAM },
   { "-xml", NULL,                   FL_OPTIONAL_DASH },
   { "-xml-file", NULL,              FL_OPTIONAL_DASH | FL_PARAM },
   { "-xterm", NULL,                 FL_OPTIONAL_DASH | FL_PARAM }
};


static const char *openmpi_bg_env_str = "-x LD_AUDIT=%s -x LDCS_LOCATION=%s -x LDCS_NUMBER=%s -x LDCS_OPTIONS=%s LDCS_CACHESIZE=%s";
static const unsigned int openmpi_size = (sizeof(openmpi_options) / sizeof(cmdoption_t));

static cmdoption_t srun_options[] = {
   { "srun", NULL,                   FL_LAUNCHER },
   { "-A",   "--account",            FL_GNU_PARAM },
   { NULL,   "--begin",              FL_GNU_PARAM },
   { "-c",   "--cpus-per-task",      FL_GNU_PARAM },
   { NULL,   "--checkpoint",         FL_GNU_PARAM },
   { NULL,   "--checkpoint-dir",     FL_GNU_PARAM },
   { NULL,   "--comment",            FL_GNU_PARAM },
   { "-d",   "--dependency",         FL_GNU_PARAM },
   { "-D",   "--chdir",              FL_GNU_PARAM | FL_EXEDIR },
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
   { "-V",   "--version",            0 }
};
static const char *srun_bg_env_str = "--runjob-opts=--envs LD_AUDIT=%s LD_PRELOAD=%s LDCS_LOCATION=%s LDCS_NUMBER=%s LDCS_OPTIONS=%s LDCS_CACHESIZE=%s";
static const char *srun_bg_env_str_nopreload = "--runjob-opts=--envs LD_AUDIT=%s%s LDCS_LOCATION=%s LDCS_NUMBER=%s LDCS_OPTIONS=%s LDCS_CACHESIZE=%s";
static const char *srun_bg_env_bare_str = "%s --envs LD_AUDIT=%s LD_PRELOAD=%s LDCS_LOCATION=%s LDCS_NUMBER=%s LDCS_OPTIONS=%s LDCS_CACHESIZE=%s";
static const char *srun_bg_env_bare_str_preload = "%sLD_PRELOAD=%s:%s LD_AUDIT=%s LDCS_LOCATION=%s LDCS_NUMBER=%s LDCS_OPTIONS=%s LDCS_CACHESIZE=%s";
static const char *srun_bg_env_bare_str_nopreload = "%s LD_AUDIT=%s LDCS_LOCATION=%s LDCS_NUMBER=%s LDCS_OPTIONS=%s LDCS_CACHESIZE=%s";

static const unsigned int srun_size (sizeof(srun_options) / sizeof(cmdoption_t));

static cmdoption_t wreckrun_options[] = {
   { "wreckrun", NULL,               FL_LAUNCHER },
   { "-h",   "--help",               0},
   { "-n",   "--procs-per-node",     FL_GNU_PARAM }
};
static const char *wreck_bg_env_str = "";
static const unsigned int wreckrun_size = (sizeof(wreckrun_options) / sizeof(cmdoption_t));

#define JSRUN_OPTIONS                                 \
   { "-a", "--tasks_per_rs",        FL_GNU_PARAM },   \
   { "-p", "--np",                  FL_GNU_PARAM },   \
   { "-c", "--cpu_per_rs",          FL_GNU_PARAM },   \
   { "-d", "--launch_distribution", FL_GNU_PARAM },   \
   { "-g", "--gpu_per_rs",          FL_GNU_PARAM },   \
   { "-K", "--rs_per_socket",       FL_GNU_PARAM },   \
   { "-l", "--latency_priority",    FL_GNU_PARAM },   \
   { "-m", "--memory_per_rs",       FL_GNU_PARAM },   \
   { "-n", "--nrs",                 FL_GNU_PARAM },   \
   { "-r", "--rs_per_host",         FL_GNU_PARAM },   \
   { "-e", "--stdio_mode",          FL_GNU_PARAM },   \
   { "-f", "--appfile",             FL_GNU_PARAM | FL_CUSTOM_OPTION}, \
   { "-I", "--stdin_rank",          FL_GNU_PARAM },   \
   { "-k", "--stdio_stderr",        FL_GNU_PARAM },   \
   { "-o", "--stdio_stdout",        FL_GNU_PARAM },   \
   { "-t", "--stdio_input",         FL_GNU_PARAM },   \
   { "-h", "--chdir",               FL_GNU_PARAM },   \
   { "-A", "--allocate_only",       FL_GNU_PARAM },   \
   { NULL, "--erf_input",           FL_GNU_PARAM },   \
   { NULL, "--erf_output",          FL_GNU_PARAM },   \
   { NULL, "--erf_output_pidx",     FL_GNU_PARAM },   \
   { "-H", "--launch_node_task",    FL_GNU_PARAM },   \
   { "-i", "--immediate",           0},               \
   { "-J", "--use_reservation",     FL_GNU_PARAM },   \
   { "-L", "--use_spindle",         FL_GNU_PARAM | FL_CUSTOM_OPTION }, \
   { "-M", "--smpiargs",            FL_GNU_PARAM },   \
   { "-P", "--pre_post_exec",       FL_GNU_PARAM },   \
   { "-S", "--save_resources",      FL_GNU_PARAM },   \
   { "-U", "--use_resources",       FL_GNU_PARAM },   \
   { "-x", "--exclude_hosts",       FL_GNU_PARAM },   \
   { "-X", "--exit_on_error",       FL_GNU_PARAM },   \
   { "-Z", "--progress",            FL_GNU_PARAM },   \
   { NULL, "--print_placement",     FL_GNU_PARAM },   \
   { "-b", "--bind",                FL_GNU_PARAM },   \
   { "-D", "--env_no_propagate",    FL_GNU_PARAM },   \
   { "-E", "--env",                 FL_GNU_PARAM },   \
   { "-F", "--env_eval",            FL_GNU_PARAM },   \
   { "-?", "--help",                0},               \
   { NULL, "--usage",               0},               \
   { "-V", "--version",             0}

static cmdoption_t jsrun_options[] = {
   { "jsrun", NULL,                 FL_LAUNCHER },
   JSRUN_OPTIONS
};

static const char *jsrun_bg_env_str = "";
static const unsigned int jsrun_size = (sizeof(jsrun_options) / sizeof(cmdoption_t));

static cmdoption_t lrun_options[] = {
   { "lrun", NULL,                 FL_LAUNCHER },
   { "-n", NULL,                   FL_GNU_PARAM },
   { "-T", NULL,                   FL_GNU_PARAM },
   { "-1", NULL,                   0 },
   { "-N", NULL,                   FL_GNU_PARAM },
   { NULL, "--adv_map",            0 },
   { NULL, "--threads",            FL_GNU_PARAM },
   { NULL, "--smt",                FL_GNU_PARAM },
   { NULL, "--pack",               0 },
   { NULL, "--mpibind",            FL_GNU_PARAM },
   { "-c", NULL,                   FL_GNU_PARAM },
   { "-g", NULL,                   FL_GNU_PARAM },
   { "-W", NULL,                   FL_GNU_PARAM },
   { NULL, "--bind",               FL_GNU_PARAM },
   { NULL, "--mpibind",            FL_GNU_PARAM },
   { NULL, "--gpubind",            FL_GNU_PARAM },
   { NULL, "--core",               FL_GNU_PARAM },
   { NULL, "--core_delay",         FL_GNU_PARAM },
   { NULL, "--core_cpu",           FL_GNU_PARAM },
   { NULL, "--core_gpu",           FL_GNU_PARAM },
   { NULL, "--core_kill",          FL_GNU_PARAM },
   { "-X", NULL,                   FL_GNU_PARAM },
   { "-v", NULL,                   0 },
   { "-vv", NULL,                  0 },
   { "-vvv", NULL,                 0 },
   JSRUN_OPTIONS
};

static const char *lrun_bg_env_str = "";
static const unsigned int lrun_size = (sizeof(lrun_options) / sizeof(cmdoption_t));

static cmdoption_t marker_options[] = {
   { NULL, NULL, 0 }
};
static const char *marker_bg_env_str = "";
static const unsigned int marker_size = 1;

static ExeTest exetest;

/**
 * Turn a table of options into the a map we can use to do lookups.
 **/
LauncherParser::LauncherParser(cmdoption_t *options, size_t options_size, string bg_string, string name_, int code_) :
   bg_addenv_str(bg_string),
   name(name_),
   code(code_)
{
   for (cmdoption_t *opt = options; opt < options+options_size; opt++) {
      if (opt->flags & FL_LAUNCHER) {
         if (opt->opt)
            launcher_cmds.insert(opt->opt);
         if (opt->long_opt)
            launcher_cmds.insert(opt->long_opt);
         continue;
      }

      set<string> names;      
      if (opt->opt) {
         names.insert(opt->opt);
         if ((opt->flags & FL_OPTIONAL_DASH) &&  opt->opt[0] == '-' && opt->opt[1] != '-')
            names.insert(string("-") + opt->opt);
         if ((opt->flags & FL_OPTIONAL_DASH) &&  opt->opt[0] == '-' && opt->opt[1] == '-')
            names.insert(opt->opt+1);
      }
      if (opt->long_opt) {
         names.insert(opt->long_opt);
         if ((opt->flags & FL_OPTIONAL_DASH) && opt->long_opt[0] == '-' && opt->long_opt[1] != '-')
            names.insert(string("-") + opt->long_opt);
         else if ((opt->flags & FL_OPTIONAL_DASH) && opt->long_opt[0] == '-' && opt->long_opt[1] == '-')
            names.insert(opt->long_opt+1);
      }

      for (set<string>::iterator i = names.begin(); i != names.end(); i++) {
         arg_list.insert(make_pair(*i, opt));
      }
   }
}

LauncherParser::~LauncherParser()
{
}

/**
 * Returns true if the launcher is present in the command line
 **/
bool LauncherParser::valid(int argc, char **argv)
{
   if (!usesLauncher())
      return true;

   for (int i = 0; i < argc; i++) {
      if (isLauncher(argc, argv, i))
         return true;
   }
   return false;
}

/**
 * Should act like valid, but takes a more indepth check of the 
 * launcher (check symbols in binaries, or invoke launcer --help
 * to get launcher type).
 **/
bool LauncherParser::valid2(int /*argc*/, char** /*argv*/)
{
   return true;
}

/**
 * Returns true if this launcher uses an executable to launch code
 * Can be overwritten by child LauncherParsers, and will probably 
 * only be 'false' for the Serial launcher
 **/
bool LauncherParser::usesLauncher() const
{
   return true;
}

/**
 * Returns true iff the argument at argv[pos] represents a launcher
 * executable (e.g, /usr/bin/srun or mpirun)
 **/
bool LauncherParser::isLauncher(int argc, char **argv, int pos) const
{
   assert(pos < argc);
   string arg(argv[pos]);

   if (launcher_cmds.find(arg) != launcher_cmds.end())
      return true;

   size_t last_slash = arg.find_last_of('/');
   if (last_slash != string::npos) {
      string filename = string(arg, last_slash+1);
      if (launcher_cmds.find(filename) != launcher_cmds.end())
         return true;
   }

   return false;
}

bool LauncherParser::isExecutable(int argc, char **argv, int pos, const set<string> &exedirs) const
{
   assert(pos < argc);
   string arg(argv[pos]);

   return exetest.isExecutableFile(arg, exedirs);
}

/**
 * Can be overloaded by children of LauncherParser to handle custom arguments
 **/
bool LauncherParser::parseCustomArg(int /*argc*/, char** /*argv*/, int /*arg_pos*/, int& /*inc_argc*/) const
{
   assert(0); //Any launcher that specifies custom args should overload this function
   return false;
}

/**
 * Return false if a launcher argument should be stripped from the command line.
 **/
bool LauncherParser::includeArg(int argc, char **argv, int pos)
{
   if (argv[pos][0] == '\0')
      return false;
   return true;
}

bool LauncherParser::addBGEnvStr(int &pos, char **new_argv, string format, string default_libstring, string intercept_libstring, string location, string number, string options, string shmcache_size)
{
   int str_len = format.length() + options.length() + location.length() + number.length() + shmcache_size.length() + default_libstring.length() + 1 + intercept_libstring.length();
   char *bg_env = (char *) malloc(str_len);
   snprintf(bg_env, str_len, format.c_str(), default_libstring.c_str(), intercept_libstring.c_str(), location.c_str(), number.c_str(), options.c_str(), shmcache_size.c_str());
   new_argv[pos++] = bg_env;
   return true;
}

/**
 * If the argument at argv[pos] is a command line argument to the launcher then
 * return the cmdoption_t that goes with it
 **/
cmdoption_t *LauncherParser::getArg(int argc, char **argv, int pos) const
{
   assert(pos < argc);
   string arg(argv[pos]);

   map<string, cmdoption_t*>::const_iterator result;
   result = arg_list.find(arg);
   if (result != arg_list.end())
      return result->second;

   size_t equal_pos = arg.find('=');
   if (equal_pos != string::npos) {
      string substr = string(arg, 0, equal_pos);
      result = arg_list.find(substr);
      if (result != arg_list.end()) {
         cmdoption_t *ret = result->second;
         if ((ret->flags & FL_GNU_PARAM) &&
             (substr == string(ret->long_opt)))
         {
            return ret;
         }
      }
   }

   return NULL;
}

/**
 * Returns true if the string represents a number
 **/
bool LauncherParser::isIntegerString(string str) const
{
   bool ishex = false;
   unsigned int i = 0;
   if (str[i] == '-' || str[i] == '+')
      i++;
   if (str[i] == '0' && str[i+1] == 'x') {
      ishex = true;
      i += 2;
   }
   for (; i < str.length(); i++) {
      if (str[i] >= '0' && str[i] <= '9')
         continue;
      if (ishex && 
          ((str[i] >= 'a' && str[i] <= 'f') ||
           (str[i] >= 'A' && str[i] <= 'F')))
         continue;
      return false;
   }
   return true;
}

/** 
 * If the argument at argv[arg_pos] at opt takes extra values (e.g, -nnodes 4) then return the values
 * passed to that argument in argOptions.
 **/
bool LauncherParser::getArgOptions(int argc, char **argv, int arg_pos, cmdoption_t *opt,
                                   vector<string> &argOptions, int &inc_argc) const
{
   assert(arg_pos < argc);
   string arg(argv[arg_pos]);
   inc_argc = 0;

   if (!(opt->flags & (FL_GNU_PARAM | FL_PARAM | FL_PARAM2)))
      return true;

   string long_opt = opt->long_opt ? string(opt->long_opt) : string();
   if (opt->flags & FL_GNU_PARAM && !long_opt.empty() && arg.substr(0, long_opt.length()) == long_opt) {
      //GNU Style --arg=val.  Check for val
      size_t equal_pos = arg.find('=');
      if (equal_pos != string::npos) {
         string val(arg, equal_pos+1);
         argOptions.push_back(val);
      }
      return true;
   }

   if (opt->flags & (FL_GNU_PARAM | FL_PARAM | FL_PARAM2)) {
      //In -arg val style.  Check for val.
      if (arg_pos+1 < argc) {
         string val = argv[arg_pos+1];
         argOptions.push_back(val);
      }
   }
   if (opt->flags & FL_PARAM2) {
      //In -arg val1 val2 style.  We already got val1.  Get val2
      if (arg_pos+2 < argc) {
         string val = argv[arg_pos+2];
         argOptions.push_back(val);
      }      
   }

   inc_argc = argOptions.size();

   if (!(opt->flags & FL_OPTIONAL)) {
      if (argOptions.empty() || (argOptions.size() == 1 && (opt->flags & FL_PARAM2))) {
         //Non-optional vals.  But we didn't find any.
         return false;
      }
      return true;
   }

   if (opt->flags & FL_INTEGER) {
      //Optional flags that are numbers.  Check that we have numbers
      for (unsigned int i = 0; i < argOptions.size(); i++) {
         if (!isIntegerString(argOptions[i])) {
            argOptions.resize(i);
            inc_argc = i;
            break;
         }
      }
   }

   //Check that optional flags aren't other flags
   for (unsigned int i = 0; i < argOptions.size(); i++) {
      if (getArg(argc, argv, arg_pos + i + 1) != NULL) {
         argOptions.resize(i);
         inc_argc = i;
         break;
      }
   }

   return true;
}

string LauncherParser::getName() const
{
   return name;
}

string LauncherParser::getBGString() const
{
   return bg_addenv_str;
}

int LauncherParser::getCode() const
{
   return code;
}

SRunParser::SRunParser(cmdoption_t *options, size_t options_size, string bg_string, string name_, int code_) :
   LauncherParser(options, options_size, bg_string, name_, code_)
{
}

SRunParser::~SRunParser()
{
}

bool SRunParser::addBGEnvStr(int &pos, char **new_argv, string, string default_libstring, string intercept_libstring, string location, string number, string options, string shmcache_size)
{
   bool is_subaudit = atoi(options.c_str()) & OPT_SUBAUDIT;
   for (int i = 0; i < pos; i++) {
      if (strstr(new_argv[i], "--runjob-opts=") != new_argv[i])
         continue;
      
      int newlen = strlen(new_argv[i]) + strlen(srun_bg_env_bare_str_preload) + default_libstring.length() + 
         intercept_libstring.length() + location.length() + number.length() + options.length() + 
         shmcache_size.length() + 1;
      char *newstr = (char *) malloc(newlen);
      char *ld_preload_begin = strstr(new_argv[i], "LD_PRELOAD=");

      if (!is_subaudit) {
         snprintf(newstr, newlen, srun_bg_env_bare_str_nopreload, new_argv[i], default_libstring.c_str(),
                  location.c_str(), number.c_str(), options.c_str(), shmcache_size.c_str());
      }
      else if (ld_preload_begin) {
         //Already have LD_PRELOAD.  Add to LD_PRELOAD and runjob_opts
         char *origstr = new_argv[i];
         *ld_preload_begin = '\0';
         char *envval = strchr(ld_preload_begin + 1, '=') + 1;
         snprintf(newstr, newlen, srun_bg_env_bare_str_preload, origstr, intercept_libstring.c_str(), 
                  envval, default_libstring.c_str(), location.c_str(), number.c_str(), options.c_str(), 
                  shmcache_size.c_str());
      }
      else {
         //No LD_PRELOAD.  Add to existing runjob_opts
         snprintf(newstr, newlen, srun_bg_env_bare_str, new_argv[i], default_libstring.c_str(),
                  intercept_libstring.c_str(), location.c_str(), number.c_str(), options.c_str(),
                  shmcache_size.c_str());
      }
      free(new_argv[i]);
      new_argv[i] = newstr;
      return true;      
   }

   string format = is_subaudit ? srun_bg_env_str : srun_bg_env_str_nopreload;
   return LauncherParser::addBGEnvStr(pos, new_argv, format, default_libstring, intercept_libstring, location, number, options, shmcache_size);
}

SerialParser::SerialParser(cmdoption_t *options, size_t options_size, string bg_string, string name_, int code_) :
   LauncherParser(options, options_size, bg_string, name_, code_)
{
}

SerialParser::~SerialParser()
{
}

bool SerialParser::usesLauncher() const
{
   return false;
}

bool SerialParser::isExecutable(int argc, char **argv, int pos, const set<string> &exedirs) const
{
   return (pos == 0);
}

OpenMPIParser::OpenMPIParser(cmdoption_t *options, size_t options_size, string bg_string, string name_, int code_) :
   LauncherParser(options, options_size, bg_string, name_, code_)
{
}

OpenMPIParser::~OpenMPIParser()
{
}

bool OpenMPIParser::parseCustomArg(int argc, char **argv, int arg_pos, int &inc_argc) const
{
   fprintf(stderr, "%s under OpenMPI is not yet supported by Spindle\n", argv[arg_pos]);
   exit(-1);
}

JSRunParser::JSRunParser(cmdoption_t *options, size_t options_size, string bg_string, string name_, int code_) :
   LauncherParser(options, options_size, bg_string, name_, code_)
{
}

JSRunParser::~JSRunParser()
{
}

bool JSRunParser::parseCustomArg(int argc, char **argv, int arg_pos, int &inc_argc) const
{
   if (strcmp(argv[arg_pos], "--use_spindle") == 0 || (strcmp(argv[arg_pos], "-L") == 0)) {
      fprintf(stderr, "Error: Do not mix spindle job launch wrapper with the jsrun spindle option %s. "
              "Use one or the other.\n", argv[arg_pos]);
   }
   else {
      fprintf(stderr, "%s under jsrun is not yet supported by Spindle\n", argv[arg_pos]);
   }
   exit(-1);
}

LRunParser::LRunParser(cmdoption_t *options, size_t options_size, string bg_string, string name_, int code_) :
   LauncherParser(options, options_size, bg_string, name_, code_)
{
}

LRunParser::~LRunParser()
{
}

bool LRunParser::parseCustomArg(int argc, char **argv, int arg_pos, int &inc_argc) const
{
   if (strcmp(argv[arg_pos], "--use_spindle") == 0 || (strcmp(argv[arg_pos], "-L") == 0)) {
      fprintf(stderr, "Error: Do not mix spindle job launch wrapper with the lrun spindle option %s. "
              "Use one or the other.\n", argv[arg_pos]);
   }
   else {
      fprintf(stderr, "%s under lrun is not yet supported by Spindle\n", argv[arg_pos]);
   }
   exit(-1);
}

MarkerParser::MarkerParser(cmdoption_t *options, size_t options_size, std::string bg_string, std::string name_, int code_) :
   LauncherParser(options, options_size, bg_string, name_, code_)
{
}

MarkerParser::~MarkerParser()
{
}

bool MarkerParser::valid(int argc, char **argv)
{
   for (int i=0; i<argc; i++) {
      if (strcmp(argv[i], "spindlemarker") == 0) {
         return true;
      }
   }
   return false;
}

bool MarkerParser::usesLauncher() const
{
   return false;
}

cmdoption_t *MarkerParser::getArg(int argc, char **argv, int pos) const
{
   for (int i=0; i<argc; i++) {
      if (strcmp(argv[i], "spindlemarker") == 0) {
         if (pos <= i)
            return marker_options;
         return NULL;
      }
   }
   assert(0); //Should be unreachable
   return NULL;
}

bool MarkerParser::isExecutable(int argc, char **argv, int pos, const std::set<std::string> &exedirs) const
{
   return (pos > 0 && strcmp(argv[pos-1], "spindlemarker") == 0);
}

bool MarkerParser::includeArg(int argc, char **argv, int pos)
{
   if (strcmp(argv[pos], "spindlemarker") == 0)
      return false;
   return LauncherParser::includeArg(argc, argv, pos);
}

void initParsers(int parsers_enabled, set<LauncherParser *> &all_parsers)
{
   if (parsers_enabled & srun_launcher) {
      if (!srunparser)
         srunparser = new SRunParser(srun_options, srun_size, srun_bg_env_str, "slurm", srun_launcher);
      all_parsers.insert(srunparser);
   }
   if (parsers_enabled & serial_launcher) {
      if (!serialparser) 
         serialparser = new SerialParser(serial_options, serial_size, serial_bg_env_str, "serial", serial_launcher);
      all_parsers.insert(serialparser);
   }
   if (parsers_enabled & openmpi_launcher) {
      if (!openmpiparser)
         openmpiparser = new OpenMPIParser(openmpi_options, openmpi_size, openmpi_bg_env_str, "openmpi", openmpi_launcher);
      all_parsers.insert(openmpiparser);
   }
   if (parsers_enabled & wreckrun_launcher) {
      if (!wreckrunparser)
         wreckrunparser = new WreckRunParser(wreckrun_options, wreckrun_size, wreck_bg_env_str, "wreckrun", wreckrun_launcher);
      all_parsers.insert(wreckrunparser);
   }
   if (parsers_enabled & jsrun_launcher) {
      if (!jsrunparser)
         jsrunparser = new JSRunParser(jsrun_options, jsrun_size, jsrun_bg_env_str, "jsrun", jsrun_launcher);
      all_parsers.insert(jsrunparser);
   }
   if (parsers_enabled & lrun_launcher) {
      if (!lrunparser)
         lrunparser = new LRunParser(lrun_options, lrun_size, lrun_bg_env_str, "lrun", lrun_launcher);
      all_parsers.insert(lrunparser);
   }
   if (parsers_enabled & marker_launcher) {
      if (!markerparser)
         markerparser = new MarkerParser(marker_options, marker_size, marker_bg_env_str, "marker", marker_launcher);
      all_parsers.insert(markerparser);
   }
}

ExeTest::ExeTest() {
   const char *path_env = getenv("PATH");
   if (!path_env)
      return;
   
   char *path_str = strdup(path_env);
   for (char *path_entry = strtok(path_str, ":"); path_entry != NULL; path_entry = strtok(NULL, ":")) {
      path.push_back(string(path_entry));
   }
   free(path_str);
}
      
bool ExeTest::isExec(string executable, string directory)
{
   if (!directory.empty() && executable[0] == '/') {
      // Absolute path, ignore directory
      directory = string();
   }
   
   if (!directory.empty()) {
      if (directory[directory.length()-1] == '/')
         executable = directory + executable;
      else
         executable = directory + string("/") + executable;
   }
   
   int result;
   struct stat buf;  
   result = stat(executable.c_str(), &buf);
   if (result == -1) {
      return false;
   }
   
   if ((buf.st_mode & S_IFLNK) == S_IFLNK) {
      // Dereference any symbolic link and try again
      char *realname = realpath(executable.c_str(), NULL);
      result = isExec(string(realname), string());
      free(realname);
      return result;
   }
   
   // Return true if not a directory and at least one execute bit is set
   return (!(buf.st_mode & S_IFDIR) && (buf.st_mode & 0111));
}

bool ExeTest::isPathExec(string executable)
{
   if (executable.empty() || executable.find('/') != string::npos) {
      /* Contains directory.  Not searched in PATH. */
      return false;
   }
   
   for (vector<string>::iterator i = path.begin(); i != path.end(); i++) {
      if (isExec(executable, *i))
         return true;
   }
   
   return false;
}

string ExeTest::getExecutablePath(string executable)
{
   if (executable.empty())
      return string();
   if (executable.find('/') != string::npos || executable[0] == '/') { 
      if (isExec(executable, string()))
         return executable;
      return string();
   }

   for (vector<string>::iterator i = path.begin(); i != path.end(); i++) {
      string directory = *i;
      if (isExec(executable, directory)) {
         if (directory[directory.length()-1] == '/')
            return directory + executable;
         return directory + string("/") + executable;
      }
   }
   
   return string();
}

bool ExeTest::isExecutableFile(string file, const set<string> &exedirs)
{
   if (isPathExec(file))
      return true;
   if (isExec(file))
      return true;
   for (set<string>::const_iterator i = exedirs.begin(); i != exedirs.end(); i++) {
      if (isExec(file, *i))
         return true;
   }
   return false;
}

