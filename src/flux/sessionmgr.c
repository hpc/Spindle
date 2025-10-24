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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "config.h"
#include "procmgr.h"
#include "fluxmgr.h"
#include "spindlemgr.h"
#include "sessionmgr.h"
#include "spindle_launch.h"

#define TIMEOUT 40

int spindle_session_start(int argc, char **argv, const char *session_dir) {
   int result, had_error, had_timeout;

   if (is_active_service("fe", session_dir) || is_active_service("be", session_dir)) {
      spindle_debug_printf(1, "Asked to start flux spindle service, but service already running\n");
      return 0;
   }

   if (fluxmgr_is_headnode()) {
      spindle_debug_printf(1, "Starting FE on head node for spindle session\n");
      result = start_service("fe", run_fe, session_dir, argc, argv);
      if (result == -1) {
         spindle_debug_printf(1, "ERROR: starting spindle fe service\n");
         return -1;
      }
   }

   spindle_debug_printf(1, "Starting BE for spindle session\n");   
   result = init_readymsg();
   if (result == -1) {
      spindle_debug_printf(1, "ERROR: starting readymsg service for spindle session\n");      
      return -1;
   }
   result = start_service("be", run_be, session_dir, argc, argv);
   if (result == -1) {
      spindle_debug_printf(1, "ERROR: starting readymsg service for spindle session\n");           
      return -1;
   }
   result = waitfor_readymsg(TIMEOUT, &had_error, &had_timeout);
   if (result == -1) {
      if (had_error) {
         spindle_debug_printf(1, "ERROR: while waiting for spindle session startup: Server exited with error\n");
      }
      else if (had_timeout) {
         spindle_debug_printf(1, "ERROR: while waiting for spindle session startup: Timeout\n");         
      }
      else {
         spindle_debug_printf(1, "ERROR: while waiting for spindle session startup: Internal error\n");
      }
      spindle_session_stop(session_dir);
      return -1;
   }
   clean_readymsg();
   return 0;
}

int spindle_session_stop(const char *session_dir)
{
   int had_error = 0, result;
   spindle_debug_printf(1, "Stopping spindle service\n");
   if (fluxmgr_is_headnode()) {
      result = stop_service("fe", session_dir);
      if (result == -1) {
         spindle_debug_printf(1, "ERROR: stopping FE service\n");
         had_error = -1;
      }
   }
   result = stop_service("be", session_dir);
   if (result == -1) {
      spindle_debug_printf(1, "ERROR: stopping BE service\n");
      had_error = -1;
   }

   return had_error;
}

char **strip_start_from_argv(int argc, char **argv)
{
   char **new_argv;
   int i;
   
   new_argv = (char **) malloc((argc - 1) * sizeof(char *));
   new_argv[0] = argv[0];
   for (i = 2; i < argc; i++) {
      new_argv[i-1] = argv[i];
   }
   return new_argv;
}

extern char *parse_location(char *loc, int number);
extern int spindle_mkdir(char *orig_path);

#if !defined(COMMPATH)
#error COMMPATH must be defined in config.h
#endif
const char *get_session_dir()
{
   int result;
   char *dir;
   dir = parse_location((char *) (COMMPATH "/spindle_session"), 0);
   if (!dir) {
      spindle_debug_printf(1, "ERROR: Could not parse directory for spindle session location from %s/spindle_session\n", COMMPATH);
      return NULL;
   }
      
   result = spindle_mkdir(dir);
   if (result == -1) {
      spindle_debug_printf(1, "ERROR: Could not mkdir spindle session location at %s\n", dir);
      free(dir);
      return NULL;
   }
   return dir;
}
