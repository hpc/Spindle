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

#include <string>
#include <map>
#include <cstring>
#include <cassert>

#include <strings.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

using namespace std;

//Seconds to live without a child
#define TIMEOUT 10

string tmpdir;
string output_fname;

void clean();
void cleanFiles();

class UniqueProcess;
class OutputLog;
class MsgReader;

UniqueProcess *lockProcess;
OutputLog *outputLog;
MsgReader *reader;

static char exitcode[8] = { 0x01, 0xff, 0x03, 0xdf, 0x05, 0xbf, 0x07, '\n' };

class UniqueProcess
{
private:
   int fd;
   string logFileLock;
   bool unique;
public:
   UniqueProcess()
   {
      unique = false;
      logFileLock = tmpdir + string("/spindle_log_lock");
      fd = open(logFileLock.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
      if (fd != -1) {
         char pid_str[32];
         snprintf(pid_str, 32, "%d", getpid());
         write(fd, pid_str, strlen(pid_str));
         unique = true;
         return;
      }
      if (errno == EEXIST)
         return;
      fprintf(stderr, "Error creating lock file %s: %s\n", logFileLock.c_str(), strerror(errno));
   }

   ~UniqueProcess()
   {
      if (fd < 0)
         return;
      close(fd);
      unlink(logFileLock.c_str());
   }

   void cleanFile() {
      if (fd < 0)
         return;
      close(fd);
      unlink(logFileLock.c_str());
      fd = -1;
   }

   bool isUnique() const 
   {
      return unique;
   }
};

class OutputLog
{
   int fd;
   string output_file;
public:
   OutputLog(string fname) :
      output_file(fname)
   {
      char hostname[1024];
      char pid[16];
      int result = gethostname(hostname, 1024);
      hostname[1023] = '\0';
      if (result != -1) {
         output_file += string(".");
         output_file += string(hostname);
      }
      snprintf(pid, 16, "%d", getpid());
      output_file += string(".");
      output_file += string(pid);
      
      fd = creat(output_file.c_str(), 0660);
      if (fd == -1) {
         fprintf(stderr, "[%s:%u] - Error opening output file %s: %s\n",
                 __FILE__, __LINE__, output_file.c_str(), strerror(errno));
         fd = 2; //stderr
      }
   }

   ~OutputLog()
   {
      if (fd != -1 && fd != 2)
         close(fd);
   }

   bool isExitCode(const char *msg1, int msg1_size, const char *msg2, int msg2_size)
   {
      if (msg1[0] != exitcode[0])
         return false;
      if (msg1_size + msg2_size != 8)
         return false;
      
      char code[8];
      unsigned i=0;
      for (i=0; i<msg1_size; i++) 
         code[i] = msg1[i];
      for (unsigned int j=0; j<msg2_size; i++, j++)
         code[i] = msg2[j];
      
      for (i = 0; i<8; i++) {
         if (code[i] != exitcode[i]) 
            return false;
      }
      return true;
   }

   void writeMessage(const char *msg1, int msg1_size, const char *msg2, int msg2_size)
   {
      if (isExitCode(msg1, msg1_size, msg2, msg2_size)) {
         /* We've received the exitcode */
         cleanFiles();
         return;
      }

      write(fd, msg1, msg1_size);
      if (msg2)
         write(fd, msg2, msg2_size);
   }
};

class MsgReader
{
private:
   static const unsigned int MAX_MESSAGE = 4096;
   static const unsigned int LISTEN_BACKLOG = 64;

   struct Connection {
      int fd;
      struct sockaddr_un remote_addr;
      bool shutdown;
      char unfinished_msg[MAX_MESSAGE];
   };

   int sockfd;
   map<int, Connection *> conns;
   char recv_buffer[MAX_MESSAGE];
   size_t recv_buffer_size, named_buffer_size;
   bool error;
   string socket_path;

   bool addNewConnection() {
      Connection *con = new Connection();
      socklen_t remote_addr_size = sizeof(struct sockaddr_un);
      con->fd = accept(sockfd, (struct sockaddr *) &con->remote_addr, &remote_addr_size);
      con->shutdown = false;
      if (con->fd == -1) {
         fprintf(stderr, "[%s:%u] - Error adding connection: %s\n", __FILE__, __LINE__, strerror(errno));
         delete con;
         return false;
      }

      int flags = fcntl(con->fd, F_GETFL, 0);
      if (flags == -1) flags = 0;
      fcntl(con->fd, F_SETFL, flags | O_NONBLOCK);

      con->unfinished_msg[0] = '\0';
      conns.insert(make_pair(con->fd, con));
      return true;
   }
   
   bool waitAndHandleMessage() {
      fd_set rset;

      for (;;) {
         FD_ZERO(&rset);
         int max_fd = 0;
         if (sockfd != -1) {
            FD_SET(sockfd, &rset);
            max_fd = sockfd;
         }
         
         for (map<int, Connection *>::iterator i = conns.begin(); i != conns.end(); i++) {
            int fd = i->first;
            FD_SET(fd, &rset);
            if (fd > max_fd)
               max_fd = fd;
         }
         
         struct timeval timeout;
         timeout.tv_sec = TIMEOUT;
         timeout.tv_usec = 0;

         if (!max_fd) {
            return false;
         }

         int result = select(max_fd+1, &rset, NULL, NULL, conns.empty() ? &timeout : NULL);
         if (result == 0) {
            return NULL;
         }
         if (result == -1) {
            fprintf(stderr, "[%s:%u] - Error calling select: %s\n", __FILE__, __LINE__, strerror(errno));
            return NULL;
         }

         if (sockfd != -1 && FD_ISSET(sockfd, &rset)) {
            addNewConnection();
         }

         for (map<int, Connection *>::iterator i = conns.begin(); i != conns.end(); i++) {
            int fd = i->first;
            if (FD_ISSET(fd, &rset)) {
               readMessage(i->second);
            }
         }

         bool foundShutdownProc;
         do {
            foundShutdownProc = false;
            for (map<int, Connection *>::iterator i = conns.begin(); i != conns.end(); i++) {
               if (i->second->shutdown) {
                  conns.erase(i);
                  foundShutdownProc = true;
                  break;
               }
            }
         } while (foundShutdownProc);
      }
   }

   bool readMessage(Connection *con)
   {
      int result = recv(con->fd, recv_buffer, MAX_MESSAGE, 0);
      if (result == -1) {
         fprintf(stderr, "[%s:%u] - Error calling recv: %s\n", __FILE__, __LINE__, strerror(errno));
         return false;
      }

      if (result == 0) {
         //A client shutdown
         map<int, Connection *>::iterator i = conns.find(con->fd);
         assert(i != conns.end());
         i->second->shutdown = true;
         if (con->unfinished_msg[0] != '\0')
            processMessage(con, "\n", 1);
         return true;
      }

      return processMessage(con, recv_buffer, result);
   }

   bool processMessage(Connection *con, const char *msg, int msg_size) {
      int msg_begin = 0;
      for (int i = 0; i < msg_size; i++) {
         if (msg[i] != '\n')
            continue;

         if (con->unfinished_msg[0] != '\0') {
            outputLog->writeMessage(con->unfinished_msg, strlen(con->unfinished_msg),
                                    msg + msg_begin, i+1 - msg_begin);
         }
         else {
            outputLog->writeMessage(msg + msg_begin, i+1 - msg_begin,
                                    NULL, 0);
         }
         con->unfinished_msg[0] = '\0';
         msg_begin = i+1;
      }

      if (msg_begin != msg_size) {
         int remaining_bytes = msg_size - msg_begin;
         strncat(con->unfinished_msg, msg + msg_begin, remaining_bytes);
      }

      return true;
   }

public:
   
   MsgReader()
   {
      error = true;

      sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
      if (sockfd == -1) {
         fprintf(stderr, "[%s:%u] - Error calling socket: %s\n", __FILE__, __LINE__, strerror(errno));
         return;
      }

      struct sockaddr_un saddr;
      bzero(&saddr, sizeof(saddr));
      int pathsize = sizeof(saddr.sun_path);
      socket_path = tmpdir + string("/spindle_log");
      saddr.sun_family = AF_UNIX;
      if (socket_path.length() > (unsigned) pathsize-1) {
         fprintf(stderr, "[%s:%u] - Socket path overflows AF_UNIX size (%d): %s\n",
                 __FILE__, __LINE__, pathsize, socket_path.c_str());
         return;
      }
      strncpy(saddr.sun_path, socket_path.c_str(), pathsize-1);

      int result = bind(sockfd, (struct sockaddr *) &saddr, sizeof(saddr));
      if (result == -1) {
         fprintf(stderr, "[%s:%u] - Error binding socket: %s\n",
                 __FILE__, __LINE__, strerror(errno));
         return;
      }

      result = listen(sockfd, LISTEN_BACKLOG);
      if (result == -1) {
         fprintf(stderr, "[%s:%u] - Error listening socket: %s\n",
                 __FILE__, __LINE__, strerror(errno));
         return;
      }

      error = false;
   }

   ~MsgReader()
   {
      for (map<int, Connection *>::iterator i = conns.begin(); i != conns.end(); i++) {
         int fd = i->first;
         close(fd);
      }
      conns.clear();
      if (sockfd != -1) {
         close(sockfd);
         unlink(socket_path.c_str());
      }
   }

   void cleanFile() {
      close(sockfd);
      unlink(socket_path.c_str());
      sockfd = -1;
   }

   bool hadError() const {
      return error;
   }

   void run()
   {
      while (waitAndHandleMessage());
   }
};

void parseArgs(int argc, char *argv[])
{
   if (argc != 3) {
      fprintf(stderr, "spindle_logd cannot be directly invoked\n");
      exit(-1);
   }
   tmpdir = argv[1];
   output_fname = argv[2];
}

void clean()
{
   if (lockProcess)
      delete lockProcess;
   lockProcess = NULL;
   if (outputLog)
      delete outputLog;
   outputLog = NULL;
   if (reader)
      delete reader;
   reader = NULL;
}

void cleanFiles()
{
   if (lockProcess)
      lockProcess->cleanFile();
   if (reader)
      reader->cleanFile();
}

void on_sig(int)
{
   clean();
   exit(0);
}

void registerCrashHandlers()
{
   signal(SIGINT, on_sig);
   signal(SIGTERM, on_sig);
}

int main(int argc, char *argv[])
{
   registerCrashHandlers();
   parseArgs(argc, argv);

   lockProcess = new UniqueProcess();
   if (!lockProcess->isUnique())
      return 0;

   outputLog = new OutputLog(output_fname);
   reader = new MsgReader();

   if (reader->hadError()) {
      fprintf(stderr, "Reader error termination\n");
      return -1;
   }

   reader->run();

   clean();
   return 0;
}
