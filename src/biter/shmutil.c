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

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

#include "shm_wrappers.h"
#include "shmutil.h"
#include "sheep.h"

int take_lock(lock_t *lock)
{
   unsigned long one = 1, result = 0;
   unsigned long count = 0;

   if (lock->id != -1 && *lock->lock == 1 && *lock->held_by == lock->id) {
      lock->ref_count++;
      return 0;
   }

   do {
      while (*lock->lock == 1) {
         count++;
         if (count > 100000) {
            usleep(10000); //.01 sec
         }
      }
      result = __sync_lock_test_and_set(lock->lock, one);
   } while (result == 1);

   if (lock->id != -1) {
      *lock->held_by = lock->id;
      lock->ref_count = 1;
   }

   return 0;
}

int test_lock(lock_t *lock) 
{
   unsigned long one = 1, result = 0;

   if (lock->id != -1 && *lock->lock == 1 && *lock->held_by == lock->id) {
      lock->ref_count++;
      return 1;
   }

   if (*lock->lock == 1)
      return 0;

   result = __sync_lock_test_and_set(lock->lock, one);
   if (result == 1)
      return 0;

   if (lock->id != -1) {
      *lock->held_by = lock->id;
      lock->ref_count = 1;
   }

   return 1;
}

int release_lock(lock_t *lock)
{
   if (lock->id != -1 && *lock->held_by != lock->id) {
      return -1;
   }

   if (lock->ref_count > 1) {
      lock->ref_count--;
      return 0;
   }
   lock->ref_count = 0;
   *lock->held_by = -1;

   MEMORY_BARRIER;

   *lock->lock = 0;
   return 0;
}

int init_shm(const char *tmpdir, size_t shm_size, int unique_number, shminfo_t **shm)
{
   static shminfo_t shminfo;
   static int initialized = 0;
   static int cached_retval = 0;

   int fd = -1, result, path_len;
   void *mem = NULL;
   char *shm_file = NULL;
   int leader = 1;

   if (initialized) {
      if (cached_retval != -1)
         *shm = &shminfo;
      return cached_retval;
   }
   initialized = 1;

   path_len = strlen(tmpdir) + 32;
   shm_file = (char *) malloc(path_len);
   if (!shm_file) {
      goto error;
   }

   snprintf(shm_file, path_len, "/biter_shm.%d", unique_number);
   shm_file[path_len-1] = '\0';

   fd = shm_open_wrapper(shm_file, O_RDWR | O_CREAT | O_EXCL, 0600);
   if (fd == -1 && errno == EEXIST) {
      fd = shm_open_wrapper(shm_file, O_RDWR, 0600);
      leader = 0;
   }

   if (fd == -1) {
      goto error;
   }

   result = ftruncate(fd, shm_size);
   if (result == -1) {
      goto error;
   }

   mem = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
   if (mem == MAP_FAILED) {
      goto error;
   }

   shminfo.fd = fd;
   shminfo.mem = mem;
   shminfo.filename = shm_file;
   shminfo.size = shm_size;
   shminfo.shared_header = (header_t *) mem;
   shminfo.leader = leader;
   *shm = &shminfo;

   cached_retval = 0;
   return 0;

  error:
   
   if (mem)
      munmap(mem, shm_size);
   if (fd != -1) {
      close(fd);
      shm_unlink_wrapper(shm_file);
   }
   if (shm_file)
      free(shm_file);

   cached_retval = -1;
   return -1;
}

int init_heap_lock(shminfo_t *shminfo)
{
   static int initialized = 0;
   if (initialized)
      return 0;
   initialized = 1;

   shminfo->mem_lock.lock = shminfo->shared_header->base.locks + 0;
   shminfo->mem_lock.held_by = shminfo->shared_header->base.locks + 1;
   shminfo->mem_lock.id = -1;
   return 0;
}

int setup_ids(shminfo_t *shminfo)
{
   static int initialized = 0;
   if (initialized)
      return 0;
   initialized = 1;

   take_lock(&shminfo->mem_lock);
   shminfo->id = shminfo->shared_header->base.cur_id++;
   release_lock(&shminfo->mem_lock);

   shminfo->mem_lock.id = shminfo->id;

   return 0;
}

int init_heap(shminfo_t *shminfo)
{
   static int initialized = 0;
   int result;
   int pagesize = getpagesize();

   if (initialized)
      return 0;
   initialized = 1;


   result = take_heap_lock(shminfo);
   if (result == -1)
      return -1;

   init_sheep(((unsigned char *) shminfo->mem) + pagesize, shminfo->size - pagesize, 0);

   result = release_heap_lock(shminfo);
   if (result == -1)
      return -1;


   return 0;
}

int take_heap_lock(shminfo_t *shminfo)
{
   return take_lock(&shminfo->mem_lock);
}

int release_heap_lock(shminfo_t *shminfo)
{
   return release_lock(&shminfo->mem_lock);
}

void update_shm_id(shminfo_t *shminfo)
{
   shminfo->id = -1;
   take_lock(&shminfo->mem_lock);
   shminfo->id = shminfo->shared_header->base.cur_id++;
   release_lock(&shminfo->mem_lock);
   shminfo->leader = 0;
}
