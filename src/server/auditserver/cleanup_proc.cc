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
#include "spindle_debug.h"

#include "ldcs_api.h"

#include <set>
#include <string>
#include <cstring>
#include <cassert>
#include <vector>
#include <algorithm>

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

static bool longest_str_first(const string &a, const string &b)
{
   return a.size() > b.size();
}

static void rmDirSet(const set<string> &dirs, const char *prefix_dir)
{
   string path_sep("/");
   size_t prefix_size = strlen(prefix_dir);
   
   for (set<string>::const_iterator i = dirs.begin(); i != dirs.end(); i++) {
      DIR *dir = opendir(i->c_str());
      if (!dir)
         continue;
      struct dirent *dp;
      while ((dp = readdir(dir))) {
         if (dp->d_name[0] == '.' && dp->d_name[1] == '\0')
            continue;
         if (dp->d_name[0] == '.' && dp->d_name[1] == '.' && dp->d_name[2] == '\0')
            continue;

         string componentpath = *i + path_sep + dp->d_name;
         if (dirs.find(componentpath) != dirs.end())
            continue;

         if (strncmp(prefix_dir, componentpath.c_str(), prefix_size) != 0) {
            err_printf("Tried to clean a file %s that wasn't in our prefix %s\n", componentpath.c_str(), prefix_dir);
            continue;
         }
         unlink(componentpath.c_str());
      }
   }

   vector<string> ordered_dirs(dirs.begin(), dirs.end());
   sort(ordered_dirs.begin(), ordered_dirs.end(), longest_str_first);
   for (vector<string>::iterator i = ordered_dirs.begin(); i != ordered_dirs.end(); i++) {
      if (strncmp(prefix_dir, i->c_str(), prefix_size) != 0) {
         err_printf("Tried to rmdir directory %s that wasn't in our prefix %s\n", i->c_str(), prefix_dir);
         continue;
      }      
      rmdir(i->c_str());
   }   
}

class CleanupProc
{
   friend void init_cleanup_proc(const char *);
private:
   set<string> dirs;
   int write_dir_fd;
   int read_dir_fd;
   bool has_error;
   pid_t child_pid;
   const char *prefix_dir;

   CleanupProc(const char *pd);
   void rmDirs();
   void cleanupMain();
public:
   void addDir(const char *dir);
   void triggerCleanup();
   bool hadError();
};

CleanupProc::CleanupProc(const char *pd) :
   write_dir_fd(-1),
   read_dir_fd(-1),
   has_error(false),
   prefix_dir(pd)
{
   int fds[2];
   int result;

   debug_printf("Enabling dedicated process for file cleanup\n");
   result = pipe2(fds, O_CLOEXEC);
   if (result == -1) {
      err_printf("Spindle error creating pipes for cleanup proc\n");
      has_error = true;
      return;
   }
   read_dir_fd = fds[0];
   write_dir_fd = fds[1];

   child_pid = fork();
   if (child_pid) {
      close(read_dir_fd);
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

void CleanupProc::triggerCleanup()
{
   debug_printf("Cleaning up BE files using dedicated cleanup process\n");
   close(write_dir_fd);

   int status, result;
   for (;;) {
      result = waitpid(child_pid, &status, 0);
      if (result == -1) {
         int error = errno;
         err_printf("Could not waitpid for cleanup proc: %s\n", strerror(error));
         return;
      }
      if (WIFEXITED(status) || WIFSIGNALED(status)) {
         return;
      }
   }   
}

bool CleanupProc::hadError()
{
   return has_error;
}

void CleanupProc::rmDirs()
{
   rmDirSet(dirs, prefix_dir);
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
static set<string> local_dircache;

void init_cleanup_proc(const char *location_dir)
{
   assert(!proc);
   proc = new CleanupProc(location_dir);
   if (proc->hadError()) {
      delete proc;
      proc = NULL;
      return;
   }

   for (set<string>::iterator i = local_dircache.begin(); i != local_dircache.end(); i++) {
      proc->addDir(i->c_str());
   }
}

void track_mkdir(const char *dir)
{
   if (proc)
      proc->addDir(dir);
   local_dircache.insert(string(dir));
}

int lookup_prev_mkdir(const char *dir)
{
   string s(dir);
   set<string>::iterator i = local_dircache.find(s);
   return (i != local_dircache.end()) ? 1 : 0;
}

void cleanup_created_dirs(const char *prefix_dir)
{
   if (proc) {
      proc->triggerCleanup();
   }
   else {
      debug_printf("Cleaning files with local unlink/rmdirs.\n");
      rmDirSet(local_dircache, prefix_dir);
   }      
}
