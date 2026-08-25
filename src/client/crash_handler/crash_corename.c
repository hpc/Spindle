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

/* Predicts the name of the core file the kernel will write for a crash.
 * This is called by the signal handler and so must be async-signal-safe. */ 

#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/prctl.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "crash_corename.h"

#define MAX_PATH_LEN 4096

static pid_t core_gettid()
{
   return syscall(SYS_gettid);
}

static int read_line(int fd, char *line, size_t line_size)
{
   int result;
   char c;
   int pos = 0;

   for (;;) {
      do {
         result = read(fd, &c, 1);
      } while (result == -1 && errno == EINTR);
      if (result == -1)
         return -1;
      else if (result == 0) {
         line[pos] = '\0';
         return pos;
      }
      else if (c == '\n') {
         line[pos] = '\0';
         return pos;
      }
      else if (pos < line_size-1) {
         line[pos] = c;
         pos++;
      }
   }
}

static int concat_str(char *corename, size_t corename_size, int *pos, char *str)
{
   int i = 0;
   if (*pos >= corename_size)
      return -1;
   while (str[i] != '\0') {
      if (*pos >= corename_size)
         return -1;
      corename[(*pos)++] = str[i++];
   }
   return 0;
}

/* The kernel substitutes text with '/' replaced by '!' */
static int concat_esc_str(char *corename, size_t corename_size, int *pos, char *str)
{
   int i;
   for (i = 0; str[i] != '\0'; i++)
      if (str[i] == '/') str[i] = '!';
   return concat_str(corename, corename_size, pos, str);
}

static int concat_long(char *corename, size_t corename_size, int *pos, unsigned long l)
{
   long last_digit, remainder;
   int result;

   last_digit = l % 10;
   remainder = l / 10;

   if (remainder) {
      result = concat_long(corename, corename_size, pos, remainder);
      if (result == -1)
         return -1;
   }
   if (*pos >= corename_size)
      return -1;
   corename[(*pos)++] = '0' + last_digit;
   return 0;
}

static int concat_status_str(char *corename, size_t corename_size, int *pos, char *filepath, char *key)
{
   char line[256];
   int result, line_pos = 0, end_pos;

   int fd = open(filepath, O_RDONLY);
   if (fd == -1) {
      return 0;
   }

   for (;;) {
      result = read_line(fd, line, sizeof(line));
      if (result == -1 || result == 0) {
         close(fd);
         return 0;
      }
      if (strncmp(line, key, strlen(key)) != 0 || line[strlen(key)] != ':') {
         continue;
      }
      close(fd);
      line_pos = strlen(key);
      while (line[line_pos] == ':' || line[line_pos] == ' ' || line[line_pos] == '\t') line_pos++;
      end_pos = line_pos;
      while (line[end_pos] >= '0' && line[end_pos] <= '9') end_pos++;
      line[end_pos] = '\0';
      return concat_str(corename, corename_size, pos, line+line_pos);
   }
}

static int core_name_default(char *corename, size_t corename_size, int core_uses_pid)
{
   int pos = 0;
   if (!core_uses_pid) {
      concat_str(corename, corename_size, &pos, "core");
   }
   else {
      concat_str(corename, corename_size, &pos, "core.");
      concat_long(corename, corename_size, &pos, (long) getpid());
   }
   return 0;
}

int crash_corename_predict(int sig, char *corename, size_t corename_size)
{
   char pattern[256];
   int fd, result, i, j, k;
   char str[MAX_PATH_LEN+1], *slash, *exe;
   ssize_t sresult;
   struct utsname utsbuf;
   struct rlimit rlim;
   struct timeval tv;
   int added_pid = 0, core_uses_pid = 0;
   int err = 0;

   memset(corename, 0, corename_size);

   //Read contents of /proc/sys/kernel/core_uses_pid to see if its non-zero
   fd = open("/proc/sys/kernel/core_uses_pid", O_RDONLY);
   if (fd != -1) {
      result = read_line(fd, str, sizeof(str));
      close(fd);
      if (result > 0) {
         core_uses_pid = (str[0] != '0');
      }
   }

   //Read contents of core_pattern to pattern[]
   fd = open("/proc/sys/kernel/core_pattern", O_RDONLY);
   if (fd == -1)
      return core_name_default(corename, corename_size, core_uses_pid);
   result = read_line(fd, pattern, sizeof(pattern));
   close(fd);
   if (result <= 0)
      return core_name_default(corename, corename_size, core_uses_pid);
   pattern[sizeof(pattern)-1] = '\0';

   // We can't make a prediction if the core is piped to a program
   if (pattern[0] == '|')
      return 0;

   // Get CWD to create absolute path to core
   j = 0;
   if (pattern[0] != '/') {
      sresult = readlink("/proc/self/cwd", str, MAX_PATH_LEN);
      if (sresult > 0) {
         str[sresult] = '\0';
         concat_str(corename, corename_size, &j, str);
         err = concat_str(corename, corename_size, &j, "/");
      }
   }

   for (i = 0; pattern[i] != '\0' && j < corename_size; i++) {
      if (err == -1)
         break;
      if (pattern[i] != '%') {
         if (j >= corename_size)
            err = -1;
         else
            corename[j++] = pattern[i];
         continue;
      }

      i++;
      // A lone % at the end of the pattern is dropped
      if (pattern[i] == '\0')
         break;
      switch (pattern[i]) {
         case '%':
            err = concat_str(corename, corename_size, &j, "%");
            break;
         case 'c':
            result = getrlimit(RLIMIT_CORE, &rlim);
            if (result == -1)
               continue;
            err = concat_long(corename, corename_size, &j, (long) rlim.rlim_cur);
            break;
         case 'd':
            result = prctl(PR_GET_DUMPABLE, 0, 0, 0, 0);
            err = concat_long(corename, corename_size, &j, (long) result);
            break;
         case 'e':
            // The crashing thread's comm, not the executable name
            fd = open("/proc/thread-self/comm", O_RDONLY);
            if (fd == -1)
               continue;
            result = read_line(fd, str, sizeof(str));
            close(fd);
            if (result <= 0)
               continue;
            err = concat_esc_str(corename, corename_size, &j, str);
            break;
         case 'f':
         case 'E':
            memset(str, 0, sizeof(str));
            sresult = readlink("/proc/self/exe", str, sizeof(str));
            str[sizeof(str)-1] = '\0';
            if (pattern[i] == 'f') {
               slash = strrchr(str, '/');
               exe = slash ? slash+1 : str;
               err = concat_esc_str(corename, corename_size, &j, exe);
            }
            else {
               err = concat_esc_str(corename, corename_size, &j, str);
            }
            break;
         case 'g':
            sresult = (ssize_t) getgid();
            err = concat_long(corename, corename_size, &j, (long) sresult);
            break;
         case 'h':
            result = uname(&utsbuf);
            if (result == -1)
               continue;
            err = concat_esc_str(corename, corename_size, &j, utsbuf.nodename);
            break;
         case 'i':
            err = concat_long(corename, corename_size, &j, (long) core_gettid());
            break;
         case 'I':
            //Doesn't actually work. Documentation implies that it should, so I'm leaving it in.            
            str[0] = '\0';
            k = 0;
            concat_str(str, sizeof(str), &k, "/proc/self/task/");
            concat_long(str, sizeof(str), &k, (long) core_gettid());
            concat_str(str, sizeof(str), &k, "/status");
            err = concat_status_str(corename, corename_size, &j, str, "NStgid");
            break;
         case 'p':
            err = concat_long(corename, corename_size, &j, (long) getpid());
            added_pid = 1;
            break;
         case 'P':
            //Doesn't actually work. Documentation implies that it should, so I'm leaving it in.
            err = concat_status_str(corename, corename_size, &j, "/proc/self/status", "NSpid");
            break;
         case 's':
            err = concat_long(corename, corename_size, &j, (long) sig);
            break;
         case 't':
         case 'C':
            gettimeofday(&tv, NULL);
            err = concat_long(corename, corename_size, &j, (long) tv.tv_sec);
            break;
         case 'u':
            err = concat_long(corename, corename_size, &j, (long) getuid());
            break;
         default:
            break;
      }
   }

   if (core_uses_pid && !added_pid) {
      concat_str(corename, corename_size, &j, ".");
      err = concat_long(corename, corename_size, &j, (long) getpid());
   }

   if (err == -1 || j >= corename_size) {
      memset(corename, 0, corename_size);
   }

   return 0;
}
