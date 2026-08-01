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
#include "config_mgr.h"
#include "config_parser.h"

#if !defined(STR)
#define STR2(X) #X
#define STR(X) STR2(X)
#endif

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

#define DEFAULT_CLEAN_PROC_STR "no"
#define DEFAULT_CLEAN_PROC_INT 0

#define DEFAULT_PERSIST 0

#define SHM_DEFAULT_SIZE 0
#define SHM_MIN_SIZE 0

#define DEFAULT_MSGCACHE_BUFFER_KB 1024
#define DEFAULT_MSGCACHE_TIMEOUT_MS 100
#define DEFAULT_MSGCACHE_ON 0

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

#define DEFAULT_LOGGING_ENABLED false
static const int DISABLE_LOGGING_FLAGS = OPTION_HIDDEN;

static ConfigMap argmap("[Command Line]");
static bool done = false;

struct argp_option *build_options()
{
   initOptionsList();
   struct argp_option* argoptions = (struct argp_option*) malloc(sizeof(struct argp_option) * (Options->size() + 1));
   if (!argoptions) {
      err_printf("Failed to allocation options of size %lu\n", sizeof(struct argp_option) * (Options->size() + 1));
      return NULL;
   }
   int j = 0;
   for (list<SpindleOption>::const_iterator i = Options->begin(); i != Options->end(); i++, j++) {
      const SpindleOption &cur = *i;
      argp_option &argp = argoptions[j];

      if (cur.config_id == confCmdlineNewgroup) {
         argp.name = NULL;
         argp.key = 0;
         argp.arg = NULL;
         argp.flags = 0;
         argp.doc = strdup(cur.helptext);
         argp.group = (int) cur.cmdline_group;
         continue;
      }

      argp.name = cur.cmdline_long;
      argp.key = (int) cur.cmdline_key;
      argp.flags = 0;
      argp.group = (int) cur.cmdline_group;

      switch (cur.conftype) {
         case cvBool:
            argp.arg = strdup("true/false");
            argp.flags |= OPTION_ARG_OPTIONAL;
            break;
         case cvInteger:
         case cvString:
            argp.arg = strdup("val");
            break;
         case cvStringOptional:
            argp.arg = strdup("val");
            argp.flags |= OPTION_ARG_OPTIONAL;            
            break;
         case cvList:
            argp.arg = strdup("val:val:...");
            break;
         case cvEnum: {
            string val;
            for (list<const char *>::const_iterator k = cur.enum_values.begin(); k != cur.enum_values.end(); k++) {
               if (!val.empty()) {
                  val += string("|");
               }
               val += string(*k);
            }
            argp.arg = strdup(val.c_str());
            break;
         }
      }
      
      string help(cur.helptext);
      if (cur.default_value && cur.default_value[0] != '\0') {
         string default_text(" [Default = ");
         default_text += string(cur.default_value);
         default_text += string("]");
         help += default_text;
      }
      argp.doc = strdup(help.c_str());
   }
   argp_option &argp = argoptions[j];
   argp.name = NULL;
   argp.key = 0;
   argp.arg = NULL;
   argp.flags = 0;
   argp.doc = NULL;
   argp.group = 0;

   return argoptions;
}

void clean_options(struct argp_option *options) {
   for (int i = 0; options[i].name || options[i].key || options[i].doc; i++) {
      argp_option &argp = options[i];
      if (argp.arg)
         free(const_cast<char*>(argp.arg));
      if (argp.doc)
         free(const_cast<char*>(argp.doc));
   }
   free(options);
}

extern "C" {
   static int parse(int key, char *arg, struct argp_state *vstate);
}

static int parse(int key, char *arg, struct argp_state *vstate)
{
   struct argp_state *state = (struct argp_state *) vstate;

   state->err_stream = (FILE *) stderr;
   state->out_stream = (FILE *) stdout;

   if (done && key != ARGP_KEY_END)
      return 0;

   if (key == ARGP_KEY_ARG) {
      if (state->argc == 0) {
         debug_printf3("Arg returning success with ARGP_KEY_ARG\n");
         return 0;
      }
      debug_printf3("Arg returning failure with ARGP_KEY_ARG\n");      
      return ARGP_ERR_UNKNOWN;
   }
   if (key == ARGP_KEY_ARGS) {
      char **app_argv = state->argv + state->next;
      int app_argc = state->argc - state->next;
      argmap.setApplicationCmdline(app_argc, app_argv);
      debug_printf3("Arg parsing setting command line to start with %s\n", app_argv[0]);
      done = true;
      return 0;
   }
   if (key == ARGP_KEY_END) { 
      debug_printf3("Arg parsing returning after ARGP_KEY_END\n");
      return 0;
   }
   if (key == ARGP_KEY_NO_ARGS) {
      debug_printf3("Arg parsing returning after ARGP_KEY_NO_ARGS\n");
      if (argmap.getSessionStatus() == sstatus_start || argmap.getSessionStatus() == sstatus_end)
         return 0;
      argp_error(state, "Application command line required");
      return -1;
   }
   if (key == ARGP_KEY_FINI) {
      debug_printf3("Arg parsing returning after ARGP_KEY_FINI\n");
      return 0;
   }
   if (key == ARGP_KEY_INIT) {
      debug_printf3("Arg parsing returning after ARGP_KEY_INIT\n");
      return 0;
   }
   if (key == ARGP_KEY_SUCCESS) {
      debug_printf3("Arg parsing returning after ARGP_KEY_SUCCESS\n");
      return 0;
   }
   if (key == ARGP_KEY_ERROR) {
      debug_printf3("Arg parsing returning after ARGP_KEY_ERROR\n");
      return -1;
   }

   map<CmdlineShortOptions, const SpindleOption&>::const_iterator i = getOptionsByKey().find((CmdlineShortOptions) key);
   if (i == getOptionsByKey().end()) {
      return ARGP_ERR_UNKNOWN;
   }
   const SpindleOption &opt = i->second;
   debug_printf3("Arg parsing processing %s (%d)\n", opt.cmdline_long, (int) opt.cmdline_key);

   string value;
   if (!arg || arg[0] == '\0') {
      if (opt.conftype == cvBool)
         value = "true";
      else
         value = "";
   }
   else {
      value = arg;
   }
   string errtext;
   if (opt.config_id == confCmdlineOnly) {
      pair<bool, bool> boolresult = strToBool(value);
      if (opt.conftype == cvBool && !boolresult.first) {
         err_printf("Invalid parameter '%s' to %s\n", value.c_str(), opt.cmdline_long);
         argp_error(state, "Parameter type error for %s", opt.cmdline_long);
         return -1;
      }
      else if (opt.conftype == cvBool && boolresult.second == false) {
         //User said something like --push=false. So fine. We won't set anything.
      }
      else if (strcmp(opt.cmdline_long, "push") == 0) {
         argmap.set(confPushpull, "push", errtext);
      }
      else if (strcmp(opt.cmdline_long, "pull") == 0) {
         argmap.set(confPushpull, "pull", errtext);
      }
      else if (strcmp(opt.cmdline_long, "cobo") == 0) {
         argmap.set(confNetwork, "cobo", errtext);         
      }
      else if (strcmp(opt.cmdline_long, "security-munge") == 0) {
         argmap.set(confSecurity, "munge", errtext);
      }
      else if (strcmp(opt.cmdline_long, "security-lmon") == 0) {
         argmap.set(confSecurity, "launchmon", errtext);         
      }
      else if (strcmp(opt.cmdline_long, "security-keyfile") == 0) {
         argmap.set(confSecurity, "keyfile", errtext);         
      }
      else if (strcmp(opt.cmdline_long, "security-none") == 0) {
         argmap.set(confSecurity, "none", errtext);         
      }
      else if (strcmp(opt.cmdline_long, "launcher-startup") == 0) {
         argmap.set(confJoblauncher, "slurm", errtext);
      }
      else if (strcmp(opt.cmdline_long, "enable-hostbin") == 0) {
         argmap.set(confJoblauncher, "hostbin", errtext);
      }
      else if (strcmp(opt.cmdline_long, "no-mpi") == 0 || strcmp(opt.cmdline_long, "serial") == 0) {
         argmap.set(confJoblauncher, "serial", errtext);
      }
      else {
         err_printf("Unknown confCmdlineOnly option %s\n", opt.cmdline_long);
         argp_error(state, "Internal error parsing %s\n", opt.cmdline_long);
         return -1;
      }
      if (!errtext.empty()) {
         argp_error(state, "Type error: %s\n", errtext.c_str());
         err_printf("%s\n", errtext.c_str());
         return EINVAL;
      }
      return 0;
   }
   
   bool result = argmap.set(opt.config_id, value, errtext);
   if (!result) {
      debug_printf("Aborting arg parsing for error '%s'\n", errtext.c_str());
      argp_error(state, "%s option: %s\n", opt.cmdline_long, errtext.c_str());
      return -1;
   }
   return 0;
};

int parseArgs(int argc, char *argv[], bool inExecutable, std::string &errmsg, ConfigMap &confresult)
{
   error_t result;
   unsigned int argflags = 0;
   struct argp arg_parser;
   char *captured_io = NULL;
   size_t captured_io_size = 0;
   FILE *io = NULL;

   if (!inExecutable) 
      io = open_memstream(&captured_io, &captured_io_size);
   else
      io = stderr;
   
   argp_program_version = PACKAGE_VERSION;
   argp_program_bug_address = PACKAGE_BUGREPORT;
   
   done = false;
   struct argp_option *argopts = build_options();
   bzero(&arg_parser, sizeof(arg_parser));
   arg_parser.options = argopts;
   arg_parser.parser = parse;
   arg_parser.args_doc = "app_command";

   argflags = ARGP_IN_ORDER;
   if (!inExecutable)
      argflags |= ARGP_NO_EXIT;
   
   result = argp_parse(&arg_parser, argc, argv, argflags, NULL, io);
   if (!inExecutable)
      fclose(io);
   clean_options(argopts);
   if (result != 0) {
      if (captured_io) {
         errmsg = captured_io;
         free(captured_io);
         err_printf("Returning error from argp_parse: %s\n", errmsg.c_str());
      }
      return -1;
   }
   
   confresult = argmap;
   return 0;
}
