/*
This file is part of Spindle.  For copyright information see the COPYRIGHT 
file in the top level directory, or at 
https://github.com/hpc/Spindle/blob/master/COPYRIGHT

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
#include <vector>
#include <set>

using namespace std;

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "spindle_launch.h"
#include "spindle_debug.h"

#if !defined(LIBEXEC)
#error Expected libexec to be defined
#endif
static const char libexec_dir[] = LIBEXEC "/";

static sigset_t child_blocked_mask;
static sigset_t child_unblocked_mask;
static bool hit_sigchild = 0;

static bool hostbin_exit_signal = false;
static bool launcher_exit_signal = false;
static bool exit_detected = false;
static pthread_cond_t exited_cvar;
static pthread_mutex_t exited_mut;
static pthread_cond_t result_cvar;
static pthread_mutex_t result_mut;

static void on_child(int)
{
   hit_sigchild = true;
}

class IOThread {
   static const int RD = 0;
   static const int WR = 1;
   static const int READ_SIZE = 0x1000;

   int launcher_stdout_pipe[2];
   int launcher_stderr_pipe[2];
   int hostbin_launcher_pipe[2];
   int hostbin_result_pipe[2];
   int done_pipe[2];
   
   bool started;
   pthread_t thrd;
   char read_buffer[READ_SIZE];
   string partial_line;

   bool launcher_done;
   bool hostbin_done;
   vector<string> hostlist;
private:
   void mkpipe(int (&p)[2], bool set_nonblock = false) {
      int result = pipe(p);
      if (result == -1) {
         const char *errstr = strerror(errno);
         err_printf("Error creating pipes for IOThread: %s\n", errstr);
         fprintf(stderr, "Could not create pipe for startup binary: %s\n", errstr);
         exit(-1);
      }
      if (set_nonblock) {
         int orig_flags = fcntl(p[RD], F_GETFL);
         fcntl(p[RD], F_SETFL, orig_flags | O_NONBLOCK);
      }
   }

   bool copyInput(int from_fd, int to_fd1, int to_fd2, bool &read_anything) {
      ssize_t result;
      read_anything = false;
      if (from_fd == -1)
         return false;

      do {
         do {
            result = read(from_fd, read_buffer, sizeof(read_buffer));
         } while (result == -1 && errno == EINTR);
         if (result == -1) {
            char *errstr = strerror(errno);
            debug_printf("IOThread Failed to read input from %d: %s\n", from_fd, errstr);
            return false;
         }
         if (result > 0) {
            read_anything = true;
            if (to_fd1 != -1) {
               write(to_fd1, read_buffer, result);
            }
            if (to_fd2 != -1) {
               write(to_fd2, read_buffer, result);
            }
         }
      } while (result > 0);

      return true;
   }
   
   void parseHostbinStdout(int fd, bool &read_anything) {
      read_anything = false;
      ssize_t result;
      for(;;) {
         do {
            result = read(fd, read_buffer, sizeof(read_buffer));
         } while (result == -1 && errno == EINTR);
         if (result == 0 || (result == -1 && errno == EAGAIN))
            break;
         if (result == -1) {
            char *errstr = strerror(errno);
            err_printf("IOThread failed to read hostbin input from %d: %s\n", fd, errstr);
            return;
         }
         read_anything = true;
         ssize_t cur = 0, buffer_size = result;
         while (cur < buffer_size) {
            char *str = read_buffer + cur;
            unsigned int str_size = 0;
            while ((str_size + cur < buffer_size) && 
                   (str[str_size] != '\n') &&
                   (str[str_size] != '\0')) str_size++;
            
            if (str[str_size] != '\n' && str[str_size] != '\0') {
               partial_line = string(read_buffer + cur, str_size);
               return;
            }
            string host;
            if (str_size > 0)
               host = partial_line + string(read_buffer + cur, str_size);
            else 
               host = partial_line;
            cur = cur + str_size + 1;
            if (!host.empty()) {
               debug_printf3("Adding %s to hostlist from hostbin\n", host.c_str());
               hostlist.push_back(host);
            }
            if (!partial_line.empty())
               partial_line = string();
         }
      }
   }

   void handleProcessExit(bool &hostbin_exited, bool &launcher_exited)
   {
      pthread_mutex_lock(&exited_mut);
      assert(!hostbin_exit_signal && !launcher_exit_signal);
      exit_detected = true;
      pthread_cond_signal(&exited_cvar);
      pthread_mutex_unlock(&exited_mut);
      
      pthread_mutex_lock(&result_mut);
      while (!hostbin_exit_signal && !launcher_exit_signal)
         pthread_cond_wait(&result_cvar, &result_mut);
      hostbin_exited = hostbin_exit_signal;
      launcher_exited = launcher_exit_signal;
      hostbin_exit_signal = launcher_exit_signal = false;
      pthread_mutex_unlock(&result_mut);
   }

   void markLauncherDone()
   {
      launcher_done = true;
      close(launcher_stdout_pipe[RD]);
      close(launcher_stderr_pipe[RD]);
      launcher_stdout_pipe[RD] = launcher_stderr_pipe[RD] = -1;
   }

   void markHostbinDone()
   {
      hostbin_done = true;
      close(hostbin_launcher_pipe[WR]);
      close(hostbin_result_pipe[RD]);
      hostbin_launcher_pipe[WR] = hostbin_result_pipe[RD] = -1;
   }

   static void *iothread_main(void *param) 
   {
      IOThread *thr = static_cast<IOThread *>(param);
      thr->main_loop();
      return NULL;
   }

   void main_loop()
   {
      bool read_anything;
      for (;;) {
         bool launcher_done = false, hostbin_done = false;
         fd_set read_set;
         int max_fd = 0;
         FD_ZERO(&read_set);
         
#define ADD_FD(FD)                                                \
         do { if (FD != -1) {                                     \
                 FD_SET(FD, &read_set);                           \
                 if (FD > max_fd) max_fd = FD;                    \
            } } while (0)

         ADD_FD(launcher_stdout_pipe[RD]);
         ADD_FD(launcher_stderr_pipe[RD]);
         ADD_FD(hostbin_result_pipe[RD]);
         ADD_FD(done_pipe[RD]);

         if (max_fd == 0)
            break;

         hit_sigchild = false;
         int result = pselect(max_fd+1, &read_set, NULL, NULL, NULL, &child_unblocked_mask);
         if (!hit_sigchild && result == -1) {
            perror("Error calling pselect");
            exit(-1);
         }

         if (result > 0 && FD_ISSET(launcher_stderr_pipe[RD], &read_set)) {
            copyInput(launcher_stderr_pipe[RD], 2, hostbin_launcher_pipe[WR], read_anything);
            if (!read_anything)
               launcher_done = true;
         }
         if (result > 0 && FD_ISSET(launcher_stdout_pipe[RD], &read_set)) {
            copyInput(launcher_stdout_pipe[RD], 1, hostbin_launcher_pipe[WR], read_anything);
            if (!read_anything)
               launcher_done = true;
         }
         if (result > 0 && FD_ISSET(hostbin_result_pipe[RD], &read_set)) {
            parseHostbinStdout(hostbin_result_pipe[RD], read_anything);
            if (!read_anything)
               hostbin_done = true;
         }
         if (result > 0 && FD_ISSET(done_pipe[RD], &read_set)) {
            break;
         }
         if (hit_sigchild) {
            handleProcessExit(hostbin_done, launcher_done);
         }
         if (hostbin_done) {
            parseHostbinStdout(hostbin_result_pipe[RD], read_anything);
            markHostbinDone();
            continue;
         }
         if (launcher_done) {
            copyInput(launcher_stderr_pipe[RD], 2, hostbin_launcher_pipe[WR], read_anything);
            copyInput(launcher_stdout_pipe[RD], 1, hostbin_launcher_pipe[WR], read_anything);
            markLauncherDone();
         }
      }
   }
public:
   IOThread() :
      started(false),
      launcher_done(false),
      hostbin_done(false)
   {
      mkpipe(launcher_stdout_pipe, true);
      mkpipe(launcher_stderr_pipe, true);
      mkpipe(hostbin_launcher_pipe);
      mkpipe(hostbin_result_pipe, true);
      mkpipe(done_pipe);
   }

   ~IOThread() {
      close(launcher_stdout_pipe[0]);
      close(launcher_stdout_pipe[1]);
      close(launcher_stderr_pipe[0]);
      close(launcher_stderr_pipe[1]);
      close(hostbin_launcher_pipe[0]);
      close(hostbin_launcher_pipe[1]);
      close(hostbin_result_pipe[0]);
      close(hostbin_result_pipe[1]);
      
      if (started) {
         char c = 'c';
         write(done_pipe[WR], &c, sizeof(c));
         pthread_join(thrd, NULL);
      }
      
      close(done_pipe[0]);
      close(done_pipe[1]);
   }

   void setupLauncherPipesParent() {
      close(launcher_stdout_pipe[WR]);
      close(launcher_stderr_pipe[WR]);
      launcher_stdout_pipe[WR] = launcher_stderr_pipe[WR] = -1;
   }

   void setupLauncherPipesChild() {
      dup2(launcher_stdout_pipe[WR], 1);
      dup2(launcher_stderr_pipe[WR], 2);
      close(launcher_stdout_pipe[RD]);
      close(launcher_stderr_pipe[RD]);
   }

   void setupHostbinPipesParent() {
      close(hostbin_launcher_pipe[RD]);
      close(hostbin_result_pipe[WR]);
      hostbin_launcher_pipe[RD] = hostbin_result_pipe[WR] = -1;
   }

   void setupHostbinPipesChild() {
      dup2(hostbin_launcher_pipe[RD], 0);
      dup2(hostbin_result_pipe[WR], 1);
      close(hostbin_launcher_pipe[WR]);
      close(hostbin_result_pipe[RD]);
   }
   
   bool run() {
      int result = pthread_create(&thrd, NULL, iothread_main, static_cast<void*>(this));
      if (result == -1) {
         err_printf("Spindle Error: Failed to create IOThread: %s\n", strerror(errno));
         return false;
      }
      started = true;
      return true;
   }

   bool launcherDone() {
      return launcher_done;
   }

   vector<string> &getHosts() {
      return hostlist;
   }
};

class ProcManager 
{
private:
   enum proc_result_t {
      not_exited,
      proc_success,
      proc_error
   };

   IOThread *io;
   pid_t launcher_pid;
   pid_t hostbin_pid;
   bool launcher_running;
   bool hostbin_running;
   proc_result_t launcher_result;
   proc_result_t hostbin_result;
   int launcher_return;

   proc_result_t collectHostbinResult() {
      int status, result;
      result = waitpid(hostbin_pid, &status, WNOHANG);
      if (result == 0) {
         return not_exited;
      }
      if (result == -1) {
         err_printf("Unexpected error calling waitpid for hostbin: %s\n", strerror(errno));
         return proc_error;
      }
      if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
         debug_printf("Hostbin completed successfully\n");
         hostbin_running = false;
         return proc_success;
      }
      else if (WIFSIGNALED(status)) {
         err_printf("hostbin exited with signal %d\n", WTERMSIG(status));
         hostbin_running = false;
         return proc_error;
      }
      else if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
         err_printf("hostbin exited with non-zero return %d\n", WEXITSTATUS(status));
         hostbin_running = false;
         return proc_error;
      }
      else {
         err_printf("Unexpected return from waitpid on hostbin: %d\n", status);
         return proc_error;
      }
   }

   proc_result_t collectLauncherResult() {
      int status, result;
      result = waitpid(launcher_pid, &status, WNOHANG);
      if (result == 0) {
         return not_exited;
      }

      if (result == -1) {
         err_printf("Unexpected error calling waitpid for launcher: %s\n", strerror(errno));
         return proc_error;
      }
      if (WIFEXITED(status)) {
         debug_printf("Launcher returned with return code %d\n", WEXITSTATUS(status));
         launcher_return = WEXITSTATUS(status);
         launcher_running = false;
         return proc_success;
      }
      else if (WIFSIGNALED(status)) {
         err_printf("Launcher exited with signal %d\n", WTERMSIG(status));
         launcher_return = -1;
         launcher_running = false;
         return proc_success;
      }
      else {
         err_printf("waitpid returned unexpected status for launcher: %d\n", status);
         return proc_error;
      }
   }
public:
   ProcManager(IOThread *io_) :
      io(io_),
      launcher_pid(-1),
      hostbin_pid(-1),
      launcher_running(false),
      hostbin_running(false),
      launcher_result(not_exited),
      hostbin_result(not_exited),
      launcher_return(0)
   {
   }

   void runLauncher(int /*launcher_argc*/, char **launcher_argv) {
      launcher_pid = fork();
      if (launcher_pid == -1) {
         fprintf(stderr, "Spindle error: Failed to fork process for job launcher: %s\n", strerror(errno));
         exit(-1);
      }
      if (launcher_pid == 0) {
         io->setupLauncherPipesChild();
         execvp(launcher_argv[0], launcher_argv);
         char *errstr = strerror(errno);
         fprintf(stderr, "Spindle error: Could not invoke job launcher %s: %s\n", launcher_argv[0], errstr);
         exit(-1);
      }
      io->setupLauncherPipesParent();
      launcher_running = true;
   }

   void runHostbin(string hostbin) {
      char launcher_pid_str[32];
      char *args[3];

      assert(launcher_pid != -1);

      hostbin_pid = fork();
      if (launcher_pid == -1) {
         fprintf(stderr, "Spindle error: Failed to fork process for hostbin: %s\n", strerror(errno));
         exit(-1);
      }
      if (hostbin_pid == 0) {
         snprintf(launcher_pid_str, sizeof(launcher_pid_str), "%d", launcher_pid);
         args[0] = const_cast<char *>(hostbin.c_str());
         args[1] = launcher_pid_str;
         args[2] = NULL;
         io->setupHostbinPipesChild();
         execvp(args[0], args);

         fprintf(stderr, "Spindle failed to exec hostbin %s: %s\n", args[0], strerror(errno));
         kill(launcher_pid, SIGTERM);
         exit(-1);
      }
      io->setupHostbinPipesParent();
      hostbin_running = true;
   }

   bool waitForResult() {
      bool hb_exited = false, la_exited = false;
      
      pthread_mutex_lock(&exited_mut);
      while (!exit_detected) 
         pthread_cond_wait(&exited_cvar, &exited_mut);
      exit_detected = false;
      pthread_mutex_unlock(&exited_mut);

      if (hostbin_running) {
         hostbin_result = collectHostbinResult();
         hb_exited = (hostbin_result != not_exited);
      }
      if (launcher_running) {
         launcher_result = collectLauncherResult();
         la_exited = (launcher_result != not_exited);
      }

      pthread_mutex_lock(&result_mut);
      hostbin_exit_signal = hb_exited;
      launcher_exit_signal = la_exited;
      pthread_cond_signal(&result_cvar);
      pthread_mutex_unlock(&result_mut);

      return true;
   }

   bool launcherDone() {
      return launcher_result != not_exited;
   }
   
   int launcherResult() {
      return launcher_return;
   }

   bool hostbinDone() {
      return hostbin_result != not_exited;
   }

   bool hostbinError() {
      return hostbin_result == proc_error;
   }

   
};

static bool fileExists(string f)
{
   struct stat buf;
   int result = stat(f.c_str(), &buf);
   return result != -1;
}

int startHostbinFE(string hostbin_exe,
                   int app_argc, char **app_argv,
                   spindle_args_t *params)
{
   IOThread io_thread;
   ProcManager proc_manager(&io_thread);

   if (hostbin_exe.find('/') == string::npos) {
      string fullpath = string(libexec_dir) + hostbin_exe;
      if (fileExists(fullpath))
         hostbin_exe = fullpath;
   }

   signal(SIGPIPE, SIG_IGN);
   signal(SIGCHLD, on_child);
   pthread_sigmask(SIG_BLOCK, NULL, &child_blocked_mask);
   pthread_sigmask(SIG_BLOCK, NULL, &child_unblocked_mask);
   sigaddset(&child_blocked_mask, SIGCHLD);
   sigdelset(&child_unblocked_mask, SIGCHLD);
   pthread_sigmask(SIG_BLOCK, &child_blocked_mask, NULL);
   pthread_cond_init(&exited_cvar, NULL);
   pthread_mutex_init(&exited_mut, NULL);
   pthread_cond_init(&result_cvar, NULL);
   pthread_mutex_init(&result_mut, NULL);


   proc_manager.runLauncher(app_argc, app_argv);
   proc_manager.runHostbin(hostbin_exe);

   io_thread.run();

   proc_manager.waitForResult();

   if (proc_manager.launcherDone()) {
      err_printf("Job launcher terminated prematurely\n");
      return proc_manager.launcherResult();
   }
   if (proc_manager.hostbinError()) {
      err_printf("Hostbin exited with an error code.  Exiting\n");
      return -1;
   }
   assert(proc_manager.hostbinDone());
   
   vector<string> &hostvec = io_thread.getHosts();
   if (hostvec.empty()) {
      err_printf("Empty hostlist from hostbin %s\n", hostbin_exe.c_str());
      return -1;
   }

   char **host_array = (char **) malloc(sizeof(char *) * (hostvec.size()+1));
   for (unsigned int i = 0; i < hostvec.size(); i++)
      host_array[i] = const_cast<char *>(hostvec[i].c_str());
   host_array[hostvec.size()] = NULL;

   int result = spindleInitFE(const_cast<const char **>(host_array), params);
   if (result == -1) {
      debug_printf("spindleInitFE returned an error\n");
      return -1;
   }

   proc_manager.waitForResult();
   assert(proc_manager.launcherDone());

   spindleCloseFE(params);

   return proc_manager.launcherResult();
}
