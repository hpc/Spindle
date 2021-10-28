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

#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include "exitnote.h"
#include "spindle_debug.h"

#define LINUX_MAX_SUN_PATH 108
#define CONNECT_TIMEOUT 300

int badhash(const char *s) {
   int hash = 7, i;
   for (i = 0; s[i]; i++)
      hash = hash*31+s[i];
   return hash;
}

static char *exitSocketPath(const char *location)
{
   char hostname[256], *dot, *endslash;
   char socketpath[LINUX_MAX_SUN_PATH];
   int result;

   result = gethostname(hostname, sizeof(hostname));
   if (result == -1) {
      hostname[0] = '\0';
   }
   hostname[sizeof(hostname)-1] = '\0';
   dot = strchr(hostname, '.');
   if (dot)
      *dot = '\0';

   endslash = strrchr(location, '/');
   if (endslash && endslash[1] == '\0')
      endslash = "";
   else
      endslash = "/";

   result = snprintf(socketpath, sizeof(socketpath), "%s%sspindle_bext_%s", location, endslash, hostname);
   if (result > sizeof(socketpath)) {
      snprintf(socketpath, sizeof(socketpath), "%s%s%x", location, endslash, badhash(hostname));
   }
   if (result > sizeof(socketpath)) {
      err_printf("Could not fit socket path %s%sspindle_bext_%s into max socket path of %lu\n",
                 location, endslash, hostname, (unsigned long) sizeof(socketpath));
      return NULL;
   }
   socketpath[sizeof(socketpath)-1] = '\0';
   
   return strdup(socketpath);
}

int createExitNote(const char *location)
{
   char *socketpath = NULL;
   int cresult = -1, error;
   int sockfd = -1, result;
   struct sockaddr_un saddr;

   debug_printf2("Creating exit note in location %s\n", location);
   socketpath = exitSocketPath(location);
   if (!socketpath)
      goto done;
   debug_printf3("Exit note socket path is %s\n", socketpath);

   sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
   if (sockfd == -1) {
      error = errno;
      err_printf("Could not create socket: %s\n", strerror(error));
      goto done;
   }

   bzero(&saddr, sizeof(saddr));
   saddr.sun_family = AF_UNIX;
   strncpy(saddr.sun_path, socketpath, LINUX_MAX_SUN_PATH-1);

   result = bind(sockfd, (struct sockaddr *) &saddr, sizeof(saddr));
   if (result == -1) {
      error = errno;
      err_printf("Could not bind socket: %s\n", strerror(error));
      goto done;
   }

   result = listen(sockfd, 1);
   if (result == -1) {
      error = errno;
      err_printf("Could not listen to socket: %s\n", strerror(error));
      goto done;
   }

   debug_printf3("Exit note socket is %d\n", sockfd);

   cresult = sockfd;
  done:
   if (socketpath)
      free(socketpath);
   if (sockfd != -1 && cresult == -1)
      close(sockfd);
   return cresult;
}

int pingExitNote(const char *location)
{
   char *socketpath = NULL;
   int presult = -1, error, sockfd = -1, timeout, result;
   struct sockaddr_un saddr;
   char msg;

   debug_printf2("Pinging server completion at location %s\n", location);
   socketpath = exitSocketPath(location);
   if (!socketpath)
      goto done;

   sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
   if (sockfd == -1) {
      error = errno;
      err_printf("Could not create socket: %s\n", strerror(error));
      goto done;
   }

   bzero(&saddr, sizeof(saddr));
   saddr.sun_family = AF_UNIX;
   strncpy(saddr.sun_path, socketpath, LINUX_MAX_SUN_PATH-1);

   timeout = 0;
   for (;;) {
      result = connect(sockfd, (struct sockaddr *) &saddr, sizeof(struct sockaddr_un));
      if (result == -1 && (errno == ECONNREFUSED || errno == ENOENT)) {
         timeout++;
         if (timeout == CONNECT_TIMEOUT) {
            err_printf("Timed out waiting to connect to exit socket at %s\n", socketpath);
            goto done;
         }
         usleep(100000); /* .1 seconds */
      }
      else if (result == -1) {
         error = errno;
         err_printf("Could not connect to exit socket: %s\n", strerror(error));
         goto done;
      }
      else
         break;
   }

   msg = 'q';
   do {
      result = write(sockfd, &msg, 1);
   } while (result == -1 && errno == EINTR);
   if (result == -1) {
      error = errno;
      err_printf("Failed to write value to exit socket: %s\n", strerror(error));
      goto done;
   }
   if (result == 0) {
      err_printf("Socket closed before we could write to exit\n");
      goto done;
   }
      
   debug_printf3("Successfully wrote to server at location %s\n", socketpath);

   presult = 0;
  done:
   if (socketpath)
      free(socketpath);
   if (sockfd)
      close(sockfd);

   return presult;   
}

int handleExitNote(int sockfd, const char *location)
{
   int fd = -1, error, hresult = -1, result;
   socklen_t sz;
   struct sockaddr_un remote_addr;
   char msg;
   char *socketpath = NULL;

   debug_printf2("In handleExitNote(%d, %s)\n", sockfd, location);
   
   socketpath = exitSocketPath(location);
   
   sz = sizeof(struct sockaddr_un);
   fd = accept(sockfd, (struct sockaddr *) &remote_addr, &sz);
   if (fd == -1) {
      error = errno;
      err_printf("Could not accept exit socket connection: %s\n", strerror(error));
      goto done;
   }

   debug_printf3("Reading for exitNote fd %d\n", sockfd);
   do {
      result = read(fd, &msg, 1);
   } while (result == -1 && errno == EINTR);
   if (result == -1) {
      error = errno;
      err_printf("Failed to write value to exit socket: %s\n", strerror(error));
      goto done;
   }
   if (msg != 'q') {
      error = errno;
      err_printf("Recieved incorrect msg character: %c\n", msg);
      goto done;
   }
   
   debug_printf2("Successfully handled exit note\n");   
   hresult = 0;
  done:
   if (fd != -1)
      close(fd);
   if (sockfd != -1)
      close(sockfd);
   if (socketpath) {
      unlink(socketpath);
      free(socketpath);
   }
   return hresult;
}
