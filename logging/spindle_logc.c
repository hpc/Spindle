/*
This file is part of Spindle.  For copyright information see the COPYRIGHT 
file in the top level directory, or at 
<TODO:URL>.

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

#include "spindle_logc.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <errno.h>
#include <execinfo.h>

#if !defined(LIBEXEC)
#error Expected to have LIBEXEC defined
#endif

#define STR2(S) #S
#define STR(S) STR2(S)
#define LOG_DAEMON STR(LIBEXEC) "/spindle_logd"

static int fd;
FILE *spindle_debug_output_f;
char *spindle_debug_name = "UNKNOWN";
int spindle_debug_prints;

//Timeout in tenths of a second
#define SPAWN_TIMEOUT 300
#define CONNECT_TIMEOUT 100

int fileExists(char *name) 
{
   struct stat buf;
   return (stat(name, &buf) != -1);
}

void spawnLogDaemon(char *logfile, char *tempdir)
{
   int result = fork();
   if (result == 0) {
      result = fork();
      if (result == 0) {
         char *params[4];
         params[0] = LOG_DAEMON;
         params[1] = tempdir;
         params[2] = logfile;
         params[3] = NULL;
         
         execv(LOG_DAEMON, params);
         fprintf(stderr, "Error executing %s: %s\n", LOG_DAEMON, strerror(errno));
         exit(0);
      }
      else {
         exit(0);
      }
   }
   else 
   {
      int status;
      do {
         waitpid(result, &status, 0);
      } while (!WIFEXITED(status));
   }
}

int clearDaemon(char *tmpdir)
{
   int fd;
   char reset_buffer[512];
   char lock_buffer[512];
   char log_buffer[512];
   int pid;

   /* Only one process can reset the daemon */
   snprintf(reset_buffer, 512, "%s/spindle_log_reset", tmpdir);
   fd = open(reset_buffer, O_WRONLY | O_CREAT | O_EXCL, 0600);
   if (fd == -1)
      return 0;
   close(fd);

   snprintf(lock_buffer, 512, "%s/spindle_log_lock", tmpdir);
   snprintf(log_buffer, 512, "%s/spindle_log", tmpdir);

   fd = open(lock_buffer, O_RDONLY);
   if (fd != -1) {
      char pids[32], *cur = pids;
      while (read(fd, cur++, 1) == 1 && (cur - pids) < 32);
      cur = '\0';
      pid = atoi(pids);
      if (pid && kill(pid, 0) != -1) {
         /* The process exists, someone else likely re-created it */
         return 0;
      }
   }

   unlink(log_buffer);
   unlink(lock_buffer);
   unlink(reset_buffer);

   return 1;
}

int connectToLogDaemon(char *path)
{
   int result, pathsize, sockfd;
   struct sockaddr_un saddr;

   sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
   if (sockfd == -1)
      return -1;
   
   bzero(&saddr, sizeof(saddr));
   pathsize = sizeof(saddr.sun_path);
   saddr.sun_family = AF_UNIX;
   strncpy(saddr.sun_path, path, pathsize-1);

   int timeout = 0;
   for (;;) {
      result = connect(sockfd, (struct sockaddr *) &saddr, sizeof(struct sockaddr_un));
      if (result == -1 && (errno == ECONNREFUSED || errno == ENOENT)) {
         timeout++;
         if (timeout == CONNECT_TIMEOUT)
            return -1;
         usleep(100000); /* .1 seconds */
      }
      else if (result == -1) {
         fprintf(stderr, "Error connecting: %s\n", strerror(errno));
         return -1;
      }
      else {
         break;
      }
   }

   return sockfd;
}

void init_spindle_debugging(char *name, int survive_exec)
{
   char *tempdir, *socket_file, *location, *already_setup, *log_level_str;
   int socket_file_len, result, log_level;

   if (spindle_debug_prints)
      return;

   log_level_str = getenv("SPINDLE_DEBUG");
   if (!log_level_str)
      return;
   log_level = atoi(log_level_str);
   if (log_level <= 0)
      log_level = 1;

   /* Setup locations for temp and output files */
   location = "./spindle_output";
   tempdir = getenv("TMPDIR");
   if (!tempdir || !*tempdir)
      tempdir = "/tmp";

   already_setup = getenv("SPINDLE_DEBUG_SOCKET");
   if (already_setup) {
      fd = atoi(already_setup);
   }
   else {
      socket_file_len = strlen(tempdir) + strlen("/spindle_log") + 2;
      socket_file = (char *) malloc(socket_file_len);
      snprintf(socket_file, socket_file_len, "%s/spindle_log", tempdir);

      int tries = 5;
      for (;;) {
         /* If the daemon doesn't exist, create it and wait for its existance */
         if (!fileExists(socket_file)) {
            spawnLogDaemon(location, tempdir);
            
            int timeout = 0;
            while (!fileExists(socket_file) && timeout < SPAWN_TIMEOUT) {
               usleep(100000); /* .1 seconds */
               timeout++;
            }
            
            if (timeout == SPAWN_TIMEOUT) {
               free(socket_file);
               return;
            }
         }

         /* Establish connection to daemon */
         fd = connectToLogDaemon(socket_file);
         if (fd != -1)
            break;
         
         /* Handle failed connection. */
         if (--tries == 0) {
            free(socket_file);            
            return;
         }
         result = clearDaemon(tempdir);
         if (!result) {
            /* Give the process clearing the daemon a chance to finish, then
               try again */
            sleep(1);
         }
      }
      free(socket_file);
   }

   /* Set the connection to close on exec so we don't leak fds */
   if (!survive_exec) {
      int fdflags = fcntl(fd, F_GETFD, 0);
      if (fdflags < 0)
         fdflags = 0;
      fcntl(fd, F_SETFD, fdflags | O_CLOEXEC);
      unsetenv("SPINDLE_DEBUG_SOCKET");
   }
   else {
      int fdflags = fcntl(fd, F_GETFD, 0);
      if (fdflags < 0)
         fdflags = 0;
      fcntl(fd, F_SETFD, fdflags & ~O_CLOEXEC);
      char fd_str[32];
      snprintf(fd_str, 32, "%d", fd);
      setenv("SPINDLE_DEBUG_SOCKET", fd_str, 1);
   }

   /* Setup the variables */
   spindle_debug_output_f = fdopen(fd, "w");

   spindle_debug_name = name;
   spindle_debug_prints = log_level;
}

void spindle_dump_on_error()
{
   void *stacktrace[256];
   char **syms;
   int size, i;

   if (strstr(spindle_debug_name, "Client")) {
      return;
   }

   size = backtrace(stacktrace, 256);
   if (size <= 0)
      return;
   syms = backtrace_symbols(stacktrace, size);
   
   for (i = 0; i<size; i++) {
      fprintf(spindle_debug_output_f, "%p - %s\n", stacktrace[i], syms[i]);
   }
}

void fini_spindle_debugging()
{
   static char exitcode[8] = { 0x01, 0xff, 0x03, 0xdf, 0x05, 0xbf, 0x07, '\n' };
   write(fd, &exitcode, sizeof(exitcode));
}
