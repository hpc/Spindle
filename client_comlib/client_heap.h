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

#if !defined(_CLIENT_HEAP_H_)
#define CLIENT_HEAP_H_

#include <unistd.h>

/**
 * We have to use explicity locks around the malloc/free family of functions.
 * Normally glibc malloc/free implement thread safety if linked with libpthreads.
 * Our libc runs in its own lmid namespace and doesn't know whether libpthread is 
 * loaded, so we don't get a thread safe heap even if the app is multithreaded.
 **/
struct lock_t {
   volatile long lock;
   volatile pid_t held_by;
};

int lock(struct lock_t *l);
void unlock(struct lock_t *l);

extern struct lock_t heap_lock;
#define HEAP_LOCK do { if (lock(&heap_lock) == -1) assert(0); } while (0)
#define HEAP_UNLOCK unlock(&heap_lock)

/* These functions automatically lock the heap_lock */
void *spindle_malloc(size_t size);
void spindle_free(void *mem);
char *spindle_strdup(const char *str);
void *spindle_realloc(void *orig, size_t size);

#endif
