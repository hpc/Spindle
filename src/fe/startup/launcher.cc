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

#include "launcher.h"
#include "spindle_debug.h"
#include "config.h"
#include "spindle_launch.h"
#include "parse_launcher.h"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>

#include <vector>
using namespace std;

Launcher::Launcher(spindle_args_t *params_) :
   params(params_)
{
   int fds[2];
   pipe(fds);
   jobfinish_read_fd = fds[0];
   jobfinish_write_fd = fds[1];
}

Launcher::~Launcher()
{   
}

int Launcher::getJobFinishFD()
{
   return jobfinish_read_fd;
}

void Launcher::markFinished()
{
   char byte = 'b';
   int result;
   do {
      result = write(jobfinish_write_fd, &byte, sizeof(byte));
   } while (result == -1 && errno == EINTR);
   if (result == -1) {
      int error = errno;
      err_printf("Failed to mark job finished: %s\n", strerror(error));
   }
}

void Launcher::getSecondaryDaemonArgs(vector<const char *> &)
{
}

bool Launcher::setupJob(app_id_t id, int app_argc, char **app_argv)
{
   int mod_argc;
   char **mod_argv;

   ModifyArgv modargv(app_argc, app_argv, daemon_argc, daemon_argv, params);
   modargv.getNewArgv(mod_argc, mod_argv);

   return spawnJob(id, mod_argc, mod_argv);
}

#if !defined(LIBEXECDIR)
#error Expected LIBEXECDIR to be defined
#endif
static char spindle_daemon[] = LIBEXECDIR "/spindle_be";
char spindle_hostbin_arg[] = "--spindle_hostbin";

#define DAEMON_OPTS_SIZE 32
bool Launcher::setupDaemons()
{
   int i = 0;
   daemon_argv = (char **) malloc(DAEMON_OPTS_SIZE * sizeof(char *));

   //daemon_argv[i++] = "/usr/local/bin/valgrind";
   //daemon_argv[i++] = "--tool=memcheck";
   //daemon_argv[i++] = "--leak-check=full";
   daemon_argv[i++] = spindle_daemon;
   daemon_argv[i++] = const_cast<char *>(getDaemonArg());

#define STR2(X) #X
#define STR(X) STR2(X)
#define STR_CASE(X) case X: daemon_argv[i++] = const_cast<char *>(STR(X)); break
   switch (OPT_GET_SEC(params->opts)) {
      STR_CASE(OPT_SEC_MUNGE);
      STR_CASE(OPT_SEC_KEYLMON);
      STR_CASE(OPT_SEC_KEYFILE);
      STR_CASE(OPT_SEC_NULL);
   }

   char number_s[32];
   snprintf(number_s, 32, "%d", params->number);
   daemon_argv[i++] = strdup(number_s);

   vector<const char *> secondary_args;
   getSecondaryDaemonArgs(secondary_args);
   for (vector<const char *>::iterator j = secondary_args.begin(); j != secondary_args.end(); j++) {
      assert(i < DAEMON_OPTS_SIZE);
      daemon_argv[i++] = const_cast<char *>(*j);
   }

   daemon_argv[i] = NULL;
   assert(i < DAEMON_OPTS_SIZE);

   daemon_argc = i;
   daemon_argv = daemon_argv;

   debug_printf2("Daemon CmdLine: ");
   for (int j = 0; j < daemon_argc; j++) {
      bare_printf2("%s ", daemon_argv[j]);
   }
   bare_printf2("\n");

   return spawnDaemon();
}


