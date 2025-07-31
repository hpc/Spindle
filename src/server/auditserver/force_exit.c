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

#define _GNU_SOURCE
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>

#include "spindle_debug.h"
#include "ldcs_audit_server_filemngt.h"
#include "ldcs_api_listen.h"
#include "ldcs_audit_server_process.h"

static int enabled = 0;
static int fds[2];
static pid_t register_pid;
#define RD 0
#define WR 1

/**
 * Public API function. Sets up a pipe to communicate a forceExit.
 **/
int enableSpindleForceExitBE()
{
   int result;
   result = pipe2(fds, O_NONBLOCK);
   debug_printf2("force exit pipe[RD] = %d and pipe[WR] = %d\n", fds[RD], fds[WR]);
   if (result == -1) {
      err_printf("Unable to create pipe fds for force exit\n");
      return -1;
   }
   enabled = 1;
   register_pid = getpid();
   return 0;
}

/**
 * Public API function. If soft exiting, Triggers a byte on the forceExit pipe, which will be interpreted by the ldcs_listen
 * infrastructure as a signal to terminate.
 * If not soft exiting, just clean to prep for an exit call.
 **/
int spindleForceExitBE(int exit_type)
{
   int result;
   int error, savederr;

   if (exit_type == SPINDLE_EXIT_TYPE_SOFT) {
      if (!enabled)
         return -1;
      
      savederr = errno;
      
      debug_printf("Asked to force exit on BE through fd %d\n", fds[WR]);
      result = write(fds[WR], "x", 1);
      if (result == -1) {
         error = errno;
         err_printf("Unable to write to force exit file descriptor: %s\n", strerror(error));
         errno = savederr;
         return -1;
      }
      errno = savederr;
   }
   else if (exit_type == SPINDLE_EXIT_TYPE_HARD) {
      debug_printf("Exiting server through force exit\n");
      ldcs_audit_server_filemngt_clean();
   }
   else {
      return -1;
   }
   return 0;
}

/**
 * The fd returned from this function will 
 **/
int getForceExitFd()
{
   if (!enabled)
      return -1;
   if (fds[WR] != -1 && getpid() != register_pid) {
      debug_printf("Clearing write part of exitfd, which was started on other pid %d (I am %d)\n", register_pid, getpid());
      close(fds[WR]);
      fds[WR] = -1;
   }
   return fds[RD];
}

extern int set_exit_on_client_close(ldcs_process_data_t *procdata);
int forceExitCB(int fd, int serverid, void *data)
{
   serverid=serverid;
   char c;
   int result;
   ldcs_process_data_t *procdata = (ldcs_process_data_t *) data;
   if (!enabled)
      return 0;
   result = read(fd, &c, 1);
   if (result != 1) {
      debug_printf("No bytes read from force exit pipe. Setting to exit when ready\n");
      return 0;
   }
   if (c != 'x') {
      debug_printf("forceExitCB had incorrect character in it: %c (%d)\n", c, (int) c);      
   }
   debug_printf("Setting to exit when ready through force exit\n");
   set_exit_on_client_close(procdata);
   return 0;
}
