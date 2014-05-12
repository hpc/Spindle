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

#if !defined(SHMUTIL_H_)
#define SHMUTIL_H_

#include "sheep.h"

typedef struct {
   volatile unsigned long *lock;
   volatile unsigned long *held_by;
   int id;
   int ref_count;
} lock_t;

typedef struct {
   volatile int cur_id;
   unsigned long locks[2];
} base_header_t;

#include "demultiplex.h"
typedef struct {
   volatile int ready;
   int num_procs;
   sheep_ptr_t msg_table_ptr;
   msg_header_t polled_data;
   int has_polled_data;
   int heap_blocked;
   int max_rank;
   int num_ranks;
   int read_file;
   int num_started;
   unsigned long locks[6];
} biter_header_t;

typedef struct {
   unsigned long locks[2];
   sheep_ptr_t hash;
   sheep_ptr_t lru_head;
   sheep_ptr_t lru_end;
   size_t heap_used;
} shmcache_header_t;

typedef struct {
   base_header_t base;
   biter_header_t biter;
   shmcache_header_t shmcache;
} header_t;

typedef struct {
   void *mem;
   char *filename;
   header_t *shared_header;
   size_t size;
   lock_t mem_lock;
   int fd;
   int leader;
   int id;
} shminfo_t;

#define MEMORY_BARRIER __sync_synchronize()

int take_lock(lock_t *lock);
int test_lock(lock_t *lock);
int release_lock(lock_t *lock);

int init_shm(const char *tmpdir, size_t shm_size, int unique_number, shminfo_t **shminfo);
int init_heap_lock(shminfo_t *shminfo);
int init_heap(shminfo_t *shminfo);
int setup_ids(shminfo_t *shminfo);

int take_heap_lock(shminfo_t *shminfo);
int release_heap_lock(shminfo_t *shminfo);

#endif
