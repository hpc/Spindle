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

#include "client_heap.h"
#include "ldcs_api.h"

#include <sys/syscall.h>
#include <sched.h>
#include <assert.h>
#include <stdlib.h>

struct lock_t heap_lock;

static pid_t gettid()
{
   return syscall(SYS_gettid);
}

int lock(struct lock_t *l)
{
   pid_t me = gettid();
   for (;;) {
      long result = __sync_lock_test_and_set(&l->lock, 1);
      if (result == 0) {
         /* Obtained lock */
         l->held_by = me;
         return 0;
      }
      if (l->held_by == me) {
         /* Re-entering lock, likely via a signal.  We're not re-enterant,
            and we have to abort this operation. */
         err_printf("Failing to take lock because I already hold it.  Canceling operation\n");
         return -1;
      }
      sched_yield();
   }
}

void unlock(struct lock_t *l)
{
   l->held_by = 0;
   __sync_lock_release(&l->lock);
}

void *spindle_malloc(size_t size)
{
   void *result;
   HEAP_LOCK;
   result = malloc(size);
   HEAP_UNLOCK;
   return result;
}

void spindle_free(void *mem)
{
   HEAP_LOCK;
   free(mem);
   HEAP_UNLOCK;
}

char *spindle_strdup(const char *str)
{
   char *result;
   HEAP_LOCK;
   result = strdup(str);
   HEAP_UNLOCK;
   return result;
}

void *spindle_realloc(void *orig, size_t size)
{
   void *result;
   HEAP_LOCK;
   result = realloc(orig, size);
   HEAP_UNLOCK;
   return result;
}
