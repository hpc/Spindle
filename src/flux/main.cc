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


//This file is c++ just to fool the automake linker to use c++ linkage

extern "C" {
#include "sessionmgr.h"
#include "ldcs_api.h"
#include "parseloc.h"
}
#include "../fe/startup/config_mgr.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>

// Compute session directory from config (sessionpaths with commpaths fallback)
static std::string computeSessionDir(ConfigMap &config)
{
   char *first_valid = NULL;

   // Try sessionpaths first
   auto sessionpaths_pair = config.getValueString(confSessionPaths);
   if (sessionpaths_pair.first && !sessionpaths_pair.second.empty()) {
      char *sessionpaths = const_cast<char*>(sessionpaths_pair.second.c_str());
      fprintf(stderr, "Debug: Evaluating sessionpaths: %s\n", sessionpaths);
      if (getFirstValidPath(sessionpaths, &first_valid, 0) == 0 && first_valid) {
         std::string result(first_valid);
         free(first_valid);
         return result;
      }
      fprintf(stderr, "Debug: No valid sessionpaths found, trying commpaths fallback\n");
   }

   // Fallback to commpaths
   auto commpaths_pair = config.getValueString(confCommPaths);
   if (commpaths_pair.first && !commpaths_pair.second.empty()) {
      char *commpaths = const_cast<char*>(commpaths_pair.second.c_str());
      fprintf(stderr, "Debug: Evaluating commpaths for session: %s\n", commpaths);
      if (getFirstValidPath(commpaths, &first_valid, 0) == 0 && first_valid) {
         std::string result(first_valid);
         free(first_valid);
         return result;
      }
   }

   // Final fallback to $TMPDIR or /tmp
   const char *tmpdir = getenv("TMPDIR");
   if (tmpdir) {
      return std::string(tmpdir);
   }
   return std::string("/tmp");
}

int main(int argc, char *argv[])
{
   // Parse configuration to get sessionpaths
   ConfigMap config("[flux-spindle]");
   std::string errmsg;
   bool result = gatherAllConfigInfo(argc, argv, false, config, errmsg);
   if (!result) {
      fprintf(stderr, "Error parsing configuration: %s\n", errmsg.c_str());
      // Continue with defaults rather than failing completely
   }

   // Compute session directory
   std::string session_dir_str = computeSessionDir(config);
   const char *session_dir = session_dir_str.c_str();

   if (argc >= 2 && strcmp(argv[1], "start") == 0) {
      char **new_argv = strip_start_from_argv(argc, argv);
      return spindle_session_start(argc - 1, new_argv, session_dir);
   }
   else if (argc >= 2 && strcmp(argv[1], "stop") == 0) {
      return spindle_session_stop(session_dir);
   }

   fprintf(stderr, "Usage: %s {start|stop} [SPINDLE OPTIONS]\n", argc ? argv[0] : "[NULL]");
   return -1;
}
