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

#include "cleanup_proc.h"
#include "config.h"

#if defined(USE_CLEANUP_PROC)

#include "spindle_debug.h"
#include "ldcs_api.h"

#include <set>
#include <string>
#include <cstring>
#include <cassert>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>

using namespace std;

static bool hit_sigterm = false;
static void on_sigterm(int sig)
{
   hit_sigterm = true;
}

class CleanupProc
{
   friend void init_cleanup_proc();
private:
   set<string> dirs;
   int write_dir_fd;
   int read_dir_fd;
   bool has_error;

   CleanupProc();
   void rmDirs();
   void cleanupMain();
   void rmRecursive(string path);
public:
   void addDir(const char *dir);
   bool hadError();
};

CleanupProc::CleanupProc() :
   write_dir_fd(-1),
   read_dir_fd(-1),
   has_error(false)
{
   int fds[2];
   int result;

   result = pipe2(fds, O_CLOEXEC);
   if (result == -1) {
      err_printf("Spindle error creating pipes for cleanup proc\n");
      has_error = true;
      return;
   }
   read_dir_fd = fds[0];
   write_dir_fd = fds[1];
   
   //Do a gchild fork to reparent to init.
   pid_t child_pid = fork();
   if (child_pid == 0) {
      pid_t gchild_pid = fork();
      if (gchild_pid != 0) {
         _exit(0);
      }
   }
   else {
      close(read_dir_fd);
      int status;
      waitpid(child_pid, &status, 0);
      return;
   }
   
   //Create a new session so kills directed at tool process don't
   // automatically hit us.
   setsid();

   close(write_dir_fd);

   struct sigaction act;
   memset(&act, 0, sizeof(act));
   act.sa_handler = SIG_IGN;
   result = sigaction(SIGPIPE, &act, NULL);
   if (result == -1) {
      err_printf("Error setting SIGPIPE action to SIG_IGN\n");
      return;
   }

   act.sa_handler = on_sigterm;
   result = sigaction(SIGTERM, &act, NULL);
   if (result == -1) {
      err_printf("Error setting SIGPIPE action to SIG_IGN\n");
      return;
   }
      
   cleanupMain();
}

bool CleanupProc::hadError()
{
   return has_error;
}

void CleanupProc::rmRecursive(string path)
{
   struct stat buf;
   int result = stat(path.c_str(), &buf);

   if (result == -1)
      return;
   else if ((buf.st_mode & S_IFLNK) || (!(buf.st_mode & S_IFDIR))) {
      result = unlink(path.c_str());
      if (result == -1) {
         //Something's wrong, we shouldn't fail to unlink.  Stop deleting.
         _exit(0);
      }
      return;
   }
   else {
      DIR *dir = opendir(path.c_str());
      if (!dir) 
         return;
      struct dirent *dp;
      while ((dp = readdir(dir))) {
         if ((strcmp(dp->d_name, ".") == 0) || (strcmp(dp->d_name, "..") == 0))
            continue;
         string newpath = path + string("/") + dp->d_name;
         rmRecursive(newpath);
      }
      closedir(dir);
      rmdir(path.c_str());
   }
}

void CleanupProc::rmDirs()
{
   for (set<string>::iterator i = dirs.begin(); i != dirs.end(); i++) {
      rmRecursive(*i);
   }
}

void CleanupProc::cleanupMain()
{
   ssize_t result;
   char buffer[MAX_PATH_LEN+1];
   int cur = 0;
   for (;;) {
      result = read(read_dir_fd, buffer+cur, 1);
      if (result == 1) {
         if (buffer[cur] == '\0') {
            dirs.insert(buffer);
            cur = 0;
         }
         else {
            cur++;
         }
      }
      else if (hit_sigterm || result == 0) {
         break;
      }
      else if (result == -1) {
         err_printf("Spindle error reading from pipe: %s\n", strerror(errno));
         break;
      }
   }

   rmDirs();
   _exit(0);
}

void CleanupProc::addDir(const char *dir)
{
   if (!dir || dir[0] == '\0')
      return;

   debug_printf("Adding directory %s to cleanup queue\n", dir);
   size_t bytes_to_write = strlen(dir)+1;
   size_t bytes_written = 0;
   do {
      ssize_t result = write(write_dir_fd, dir + bytes_written, bytes_to_write - bytes_written);
      if (result == -1 && errno == EINTR)
         continue;
      else if (result == -1) {
         err_printf("Error writing cleanup directory %s to cleanup process: %s\n", dir, strerror(errno));
         return;
      }
      bytes_written += result;
   } while (bytes_written < bytes_to_write);
}

static CleanupProc *proc = NULL;
void init_cleanup_proc()
{
   assert(!proc);
   proc = new CleanupProc();
   if (proc->hadError()) {
      delete proc;
      proc = NULL;
      return;
   }
}

void add_cleanup_dir(const char *dir)
{
   if (!proc)
      return;
   proc->addDir(dir);
}

#else

void init_cleanup_proc()
{
}

void add_cleanup_dir(const char *dir)
{
}

#endif
