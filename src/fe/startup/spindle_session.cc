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

#if !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <string>
#include <map>

#include "parseargs.h"
#include "spindle_session.h"
#include "spindle_debug.h"

using namespace std;
static int pipefd[2];
static string session_socket;
static string session_id;
static int sock = -1;

#define SOCKET_MAX_CONNECTIONS 128
#define SOCKET_PREFIX "spin."
#define MIN(X, Y) (X < Y ? X : Y)

static void create_session_id()
{
   char *socket_name = tempnam(NULL, SOCKET_PREFIX);
   int socket_prefix_len = strlen(SOCKET_PREFIX);
   const char *id = strstr(socket_name, SOCKET_PREFIX) + MIN(socket_prefix_len, 5);

   session_id = string(id);
   session_socket = string(socket_name);
   free(socket_name);
}

static void set_session_id(string id)
{
   session_id = id;
   char *socket_name = tempnam(NULL, NULL);
   char *dir = strrchr(socket_name, '/') + 1;
   *dir = '\0';
   session_socket = string(socket_name) + SOCKET_PREFIX + id;
}

static int create_unixsocket()
{
   struct sockaddr_un local;
   int result, len;

   debug_printf("Launching unix socket for session\n");
   sock = socket(AF_UNIX, SOCK_STREAM, 0);
   if (sock == -1) {
      int error = errno;
      err_printf("Could not create socket for spindle session: %s\n", strerror(error));
      return -1;
   }

   local.sun_family = AF_UNIX;
   strncpy(local.sun_path, session_socket.c_str(), sizeof(local.sun_path)-1);
   len = sizeof(sockaddr_un);
   result = bind(sock, (struct sockaddr *) &local, len);
   if (result == -1) {
      int error = errno;
      err_printf("Could not bind socket %s for spindle session: %s\n", local.sun_path, strerror(error));
      return -1;
   }

   result = listen(sock, SOCKET_MAX_CONNECTIONS);
   if (result == -1) {
      int error = errno;
      err_printf("Could not listen on server socket %s for spindle session: %s", local.sun_path, strerror(error));
      return -1;
   }
   
   return sock;
}

int get_session_fd()
{
   return sock;
}

static int accept_unixsocket()
{
   struct sockaddr_un remote;
   socklen_t len = sizeof(remote);

   int client = accept(sock, (struct sockaddr*) &remote, &len);
   if (client == -1) {
      int error = errno;
      err_printf("Could not accept new connection: %s\n", strerror(error));
      return -1;
   }
   
   return client;
}

static int connect_to_session()
{
   sock = socket(AF_UNIX, SOCK_STREAM, 0);
   if (sock == -1) {
      int error = errno;
      err_printf("Failed create unix socket for session connection: %s\n", strerror(error));
      return -1;
   }

   struct sockaddr_un server;
   server.sun_family = AF_UNIX;
   strncpy(server.sun_path, session_socket.c_str(), sizeof(server.sun_path));
   int len = sizeof(struct sockaddr_un);
   
   int result = connect(sock, (const struct sockaddr *) &server, len);
   if (result == -1) {
      int error = errno;
      err_printf("Failed to connect to unix socket %s for session: %s\n", session_socket.c_str(), strerror(error));
      return -1;
   }

   return 0;
}

static int safe_send(int fd, void *buf, size_t buffer_size)
{
   debug_printf3("Sending message to fd %d of size %lu\n", fd, buffer_size);
   unsigned char *buffer = (unsigned char *) buf;
   size_t cur = 0;
   do {
      int result = send(fd, buffer+cur, buffer_size - cur, 0);
      if (result == -1 && errno == EINTR) {
         continue;
      }
      else if (result == -1) {
         err_printf("Failed to send message to daemon: %s\n", strerror(errno));
         return -1;
      }
      else if (result >= 0) {
         cur += result;
      }
   } while (cur < buffer_size);
   return buffer_size;
}

static int safe_recv(int fd, void *buf, size_t buffer_size)
{
   debug_printf3("Recving message from fd %d of size %lu\n", fd, buffer_size);
   unsigned char *buffer = (unsigned char *) buf;
   size_t cur = 0;
   do {
      int result = recv(fd, buffer+cur, buffer_size - cur, 0);
      if (result == -1 && errno == EINTR) {
         continue;
      }
      else if (result == -1) {
         err_printf("Failed to recv message from client: %s\n", strerror(errno));
         return -1;
      }
      else if (result >= 0) {
         cur += result;
      }
   } while (cur < buffer_size);
   return buffer_size;
}

static int send_msg(int app_argc, char **app_argv)
{
   int result;
   
   debug_printf3("Sending message with argc %d\n", app_argc);
   result = safe_send(sock, &app_argc, sizeof(app_argc));
   if (result == -1)
      return -1;

   if (app_argv) {
      for (int i = 0; i < app_argc; i++) {
         int len = strlen(app_argv[i]);
         result = safe_send(sock, &len, sizeof(len));
         if (result == -1)
            return -1;
         result = safe_send(sock, app_argv[i], len);
         if (result == -1)
            return -1;
      }
   }

   return 0;
}

static pid_t grandchild_fork()
{
   int result = pipe(pipefd);
   if (result == -1) {
      int error = errno;
      err_printf("Failed to create pipe for session: %s\n", strerror(error));
      return -1;
   }
   
   pid_t child = fork();
   if (child == -1) {
      int error = errno;
      err_printf("Failed to fork spindle session: %s\n", strerror(error));
      close(pipefd[0]);
      close(pipefd[1]);
      return -1;
   }
   else if (child > 0) {
      int status;
      result = waitpid(child, &status, 0);
      if (result == -1) {
         int error = errno;
         err_printf("Failed to waitpid for child %d in session fork: %s\n", child, strerror(error));
         close(pipefd[0]);
         close(pipefd[1]);
         return -1;
      }
      
      close(pipefd[1]);
      int gchild_pid = 0, result;
      do {
         result = read(pipefd[0], &gchild_pid, sizeof(gchild_pid));
      } while (result == -1 && errno == EINTR);
      int error = errno;
      close(pipefd[0]);
      if (result == -1 || result < (int) sizeof(gchild_pid)) {
         err_printf("Failed to read from grandchild after session fork: %s\n", strerror(error));
         return -1;
      }
      if (gchild_pid == -1) {
         err_printf("Failed to fork grandchild in child\n");
         return -1;
      }
      
      return gchild_pid;
   }

   close(pipefd[0]);

   int gchild = fork();
   if (gchild == -1) {
      int neg1 = -1;
      write(pipefd[1], &neg1, sizeof(neg1));
      close(pipefd[1]);
      _exit(0);
   }
   else if (gchild) {
      close(pipefd[1]);
      _exit(0);
   }
   
   return 0;
}

static void finish_session_startup(bool err)
{
   int pid = err ? -1 : getpid();
   write(pipefd[1], &pid, sizeof(int));
   close(pipefd[1]);
}
static app_id_t next_app_id = 1;
static map<app_id_t, int> socket_ids;

int get_session_runcmds(app_id_t &appid, int &app_argc, char** &app_argv, bool &session_complete)
{
   int arg_max = 4096;

   debug_printf("Receiving client request in session handler\n");

   int client = accept_unixsocket();
   if (client == -1) {
      debug_printf("Error accepting socket\n");
      return -1;
   }
   if (client == 0) {
      debug_printf("No clients ready now\n");
      return 0;
   }

   int argc;
   int result = safe_recv(client, &argc, sizeof(argc));
   if (result == -1) {
      close(client);
      return -1;
   }
   debug_printf3("Received argc value %d\n", argc);
   if (argc == -1) {
      debug_printf("Received session shutdown message\n");
      session_complete = true;
      app_argc = 0;
      app_argv = NULL;
      close(client);
      close(sock);
      unlink(session_socket.c_str());
      return 0;
   }
   if (argc > arg_max || argc < 0) {
      err_printf("Nonsense values for argc in client request: %d > %d\n", argc, arg_max);
      close(client);
      return -1;
   }
   app_argc = argc;
   app_argv = (char **) malloc(sizeof(char *) * (argc+1));
   for (int i = 0; i < argc; i++) {
      int arglen = 0;
      result = safe_recv(client, &arglen, sizeof(arglen));
      if (result == -1) {
         close(client);
         return -1;
      }
      if (arglen > (4*1024*1024) || (arglen < 0)) {
         err_printf("Nonsense values for argv in client request: %d\n", arglen);
         close(client);
         return -1;
      }
      char *arg = (char *) malloc(arglen+1);
      result = safe_recv(client, arg, arglen);
      if (result == -1) {
         close(client);
         return -1;
      }
      arg[arglen] = '\0';
      app_argv[i] = arg;
   }
   app_argv[argc] = NULL;
   appid = next_app_id++;
   socket_ids[appid] = client;

   return 0;
}

int init_session(spindle_args_t *args)
{
   int result;
   session_status_t sstatus = get_session_status();
   if (sstatus == sstatus_unused) {
      debug_printf("Spindle session not set\n");
      return 0;
   }

   if (sstatus == sstatus_start) {
      debug_printf("Starting new spindle session\n");

      create_session_id();
      debug_printf("New session-id is %s\n", session_id.c_str());
      debug_printf("New session socket is %s\n", session_socket.c_str());

      pid_t pid = grandchild_fork();
      if (pid == -1) {
         //Error mode
         debug_printf("Error in grandchild forking for session. exiting.\n");
         fprintf(stderr, "Error creating spindle session daemon\n");
         exit(-1);
      }
      else if (pid > 0) {
         //In original process. Print session-id and exit
         debug_printf("Session daemon startup was successful.  Exiting\n");
         setsid();
         setpgid(0, 0);
         printf("%s\n", session_id.c_str());
         exit(0);
      }
      else {
         //In child-process/daemon.  Run spindle.
         debug_printf("Creating unix socket for new session\n");
         result = create_unixsocket();
         if (result == -1) {
            finish_session_startup(true);
            exit(-1);
         }
         finish_session_startup(false);
      }
      return 0;
   }

   set_session_id(get_arg_session_id());
   debug_printf("Connecting to existing spindle session-id %s\n", session_id.c_str());
   result = connect_to_session();
   if (result == -1) {
      fprintf(stderr, "ERROR: Spindle could not connect to session %s\n", session_id.c_str());
      exit(-1);
   }

   if (sstatus == sstatus_run) {
      debug_printf("New run in spindle session-id %s\n", session_id.c_str());
      int app_argc;
      char **app_argv;
      getAppArgs(&app_argc, &app_argv);
      result = send_msg(app_argc, app_argv);
      if (result == -1) {
         fprintf(stderr, "ERROR: Spindle could not communicate with session %s\n", session_id.c_str());
         close(sock);
         exit(-1);
      }

      int rc = 0;
      result = safe_recv(sock, &rc, sizeof(rc));
      if (result == -1) {
         fprintf(stderr, "ERROR: Spindle was disconnected from session %s", session_id.c_str());
         rc = -1;
      }
      close(sock);
      exit(rc);
   }
   
   if (sstatus == sstatus_end) {
      debug_printf("Telling session-id %s to shutdown\n", session_id.c_str());
      result = send_msg(-1, NULL);
      close(sock);
      if (result == -1) {
         fprintf(stderr, "ERROR: Spindle could not communicate with session %s\n", session_id.c_str());
         exit(-1);
      }
      exit(0);
   }

   return -1;
}

void mark_session_job_done(app_id_t appid, int rc)
{
   map<app_id_t, int>::iterator i = socket_ids.find(appid);
   assert(i != socket_ids.end());
   int client = i->second;
   
   safe_send(client, &rc, sizeof(rc));
   socket_ids.erase(i);
   close(client);
}
