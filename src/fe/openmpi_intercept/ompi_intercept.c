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

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <pthread.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <setjmp.h>
#include <signal.h>

typedef struct {
   char *host_name;
   char *executable_name;
   int pid;
} MPIR_PROCDESC;

static volatile MPIR_PROCDESC **proctable_ptr = NULL;
static volatile int *proctable_size_ptr = NULL;
typedef void (*mpir_breakpoint_t)(void);
static volatile mpir_breakpoint_t mpirbreakpoint;

static sigjmp_buf env;
static volatile int doing_read = 0;

void on_sigsegv(int sig, siginfo_t *info, void *ctx)
{
   if (!doing_read)
      return;
   siglongjmp(env, -1);
}

#define IS_HOSTCHAR(X) ((X >= 'A' && X <= 'Z') || (X >= 'a' && X <= 'z') || (X >= '0' && X <= '9') || (X == '-') || (X == '.'))
static int check_proctable_entry(MPIR_PROCDESC *e)
{
   int i;
   char *c;
   if (e->pid == 0)
      return 0;
   if (e->host_name == NULL || *e->host_name == '\0')
      return 0;
   if (e->executable_name == NULL)
      return 0;
   
   for (i = 0, c = e->host_name; i < 255 && *c != '\0'; i++, c++) {
      if (!IS_HOSTCHAR(*c))
         return 0;
   }
   if (i == 255)
      return 0;

   return 1;
}

static void *monitor_main(void *arg)
{
   struct sigaction act, oact;
   int i, ready = 0;

   memset(&act, 0, sizeof(act));
   act.sa_sigaction = on_sigsegv;
   act.sa_flags = SA_SIGINFO;
   
   do {
      usleep(50000); //0.05 seconds
      
      sigaction(SIGSEGV, &act, &oact);
      
      if (sigsetjmp(env, 1) != 0) {
         //hit sigsegv
         goto done;
      }
      
      doing_read = 1;
      if (*proctable_ptr == NULL)
         goto done;
      if (*proctable_size_ptr == 0)
         goto done;

      for (i = 0; i < *proctable_size_ptr; i++) {
         if (!check_proctable_entry((MPIR_PROCDESC *) (*proctable_ptr + i)))
            goto done;
      }

      ready = 1;
     done:
      doing_read = 0;
      sigaction(SIGSEGV, &oact, NULL);
   } while (!ready);

   mpirbreakpoint();

   return NULL;
}

static void spawn_monitor_thread()
{
   pthread_attr_t attrs;
   pthread_t thrd;
   int result;

   pthread_attr_init(&attrs);
   pthread_attr_setdetachstate(&attrs, PTHREAD_CREATE_DETACHED);
   result = pthread_create(&thrd, &attrs, monitor_main, NULL);
   if (result != 0) {
      fprintf(stderr, "Spindle error in mpiexec intercept: Could not spawn monitor thread\n");
      exit(-1);
   }

   pthread_attr_destroy(&attrs);
}


static void on_load() __attribute__((constructor));
static void on_load()
{
   char filename[256];
   char *my_exec, *last_slash;
   unsigned long bp_addr, ptable_addr, ptable_size_addr;   
   int result;

   char *args = getenv("SPINDLE_OMPI_INTERCEPT");
   if (!args) {
      fprintf(stderr, "Spindle error in mpiexec intercept: SPINDLE_OMPI_INTERCEPT not set\n");
      exit(-1);
   }

   result = sscanf(args, "%255s %lx %lx %lx", filename, &bp_addr, &ptable_addr, &ptable_size_addr);
   if (result != 4) {
      fprintf(stderr, "Spindle error in mpiexec intercept: Could not parse SPINDLE_OMPI_INTERCEPT\n");
      fprintf(stderr, "result = %d, args = %s, filename = %s\n", result, args, filename);
      exit(-1);
   }

   my_exec = realpath("/proc/self/exe", NULL);
   if (!my_exec) {
      fprintf(stderr, "Spindle error in mpiexec intercept: Could not deref /proc/self/exe\n");
      exit(-1);
   }

   last_slash = strrchr(my_exec, '/');
   last_slash = last_slash ? last_slash+1 : my_exec;

   if (strcmp(last_slash, filename) != 0)
      return;

   assert(bp_addr);
   assert(ptable_addr);
   assert(ptable_size_addr);

   proctable_ptr = (volatile MPIR_PROCDESC **) ptable_addr;
   proctable_size_ptr = (int *) ptable_size_addr;
   mpirbreakpoint = (mpir_breakpoint_t) bp_addr;

   spawn_monitor_thread();
}

