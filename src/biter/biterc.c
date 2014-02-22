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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <assert.h>

#include "biterc.h"
#include "sheep.h"
#include "biter_shm.h"
#include "demultiplex.h"
#include "ids.h"

#if !defined(MAX_PATH_LEN)
#define MAX_PATH_LEN 4096
#endif

#if !defined(BITER_MAX_SESSIONS)
#define BITER_MAX_SESSIONS 1
#endif

typedef struct {
   volatile unsigned long *lock;
   volatile unsigned long *held_by;
   int id;
   int ref_count;
} lock_t;

#define MEMORY_BARRIER __sync_synchronize()

typedef struct {
   volatile int cur_id;
   volatile int ready;
   int num_procs;
   sheep_ptr_t msg_table_ptr;
   msg_header_t polled_data;
   int has_polled_data;
   int heap_blocked;
   int max_rank;
   int num_set_max_rank;
   unsigned long locks[8];
} header_t;

typedef struct biterc_session {
   char *shm_filename;
   void *shm;
   header_t *shared_header;
   size_t shm_size;
   lock_t pipe_lock;
   lock_t mem_lock;
   lock_t queue_lock;
   lock_t write_lock;
   int shm_fd;
   int id;
   int leader;
   int c2s_fd;
   int s2c_fd;
} biterc_session_t;

static biterc_session_t sessions[BITER_MAX_SESSIONS];
static int biter_cur_session = 0;

static const char *biter_lasterror = NULL;

extern int init_message(int num_procs, void *header_ptr, void *session);

static int init_shm(const char *tmpdir, size_t shm_size, biterc_session_t *session)
{
   int fd = -1, result, path_len;
   void *mem = NULL;
   char *shm_file = NULL;
   int leader = 1;
   int unique_number = biterc_get_job_id();

   path_len = strlen(tmpdir) + 32;
   shm_file = (char *) malloc(path_len);
   if (!shm_file) {
      biter_lasterror = "Unable to allocate memory for file path";
      goto error;
   }

   snprintf(shm_file, path_len, "/biter_shm.%d", unique_number);
   shm_file[path_len-1] = '\0';

   fd = biter_shm_open(shm_file, O_RDWR | O_CREAT | O_EXCL, 0600);
   if (fd == -1 && errno == EEXIST) {
      fd = biter_shm_open(shm_file, O_RDWR, 0600);
      leader = 0;
   }

   if (fd == -1) {
      biter_lasterror = "Could not open shared memory segment";
      goto error;
   }

   result = ftruncate(fd, shm_size);
   if (result == -1) {
      biter_lasterror = "Could not ftruncate shared memory segment";
      goto error;
   }

   mem = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
   if (mem == MAP_FAILED) {
      biter_lasterror = "mmap of shared memory segment failed";
      goto error;
   }

   session->shm_fd = fd;
   session->shm = mem;
   session->shm_filename = shm_file;
   session->shm_size = shm_size;
   session->shared_header = (header_t *) mem;
   session->leader = leader;

   return 0;

  error:
   
   if (mem)
      munmap(mem, shm_size);
   if (fd != -1) {
      close(fd);
      biter_shm_unlink(shm_file);
   }
   if (shm_file)
      free(shm_file);

   return -1;
}

static int init_locks(biterc_session_t *session)
{
   session->pipe_lock.lock = session->shared_header->locks + 0;
   session->pipe_lock.held_by = session->shared_header->locks + 1;
   session->pipe_lock.id = -1;
   session->queue_lock.lock = session->shared_header->locks + 2;
   session->queue_lock.held_by = session->shared_header->locks + 3;
   session->queue_lock.id = -1;
   session->mem_lock.lock = session->shared_header->locks + 4;
   session->mem_lock.held_by = session->shared_header->locks + 5;
   session->mem_lock.id = -1;
   session->write_lock.lock = session->shared_header->locks + 6;
   session->write_lock.held_by = session->shared_header->locks + 7;
   session->write_lock.id = -1;
   return 0;
}

static int take_lock(lock_t *lock)
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

static int test_lock(lock_t *lock) 
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

static int release_lock(lock_t *lock)
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

static int setup_ids(biterc_session_t *session)
{
   take_lock(&session->mem_lock);
   session->id = session->shared_header->cur_id;
   session->shared_header->cur_id++;
   release_lock(&session->mem_lock);

   session->pipe_lock.id = session->id;
   session->queue_lock.id = session->id;
   session->mem_lock.id = session->id;
   session->write_lock.id = session->id;

   return 0;
}

int take_heap_lock(void *session)
{
   return take_lock(& ((biterc_session_t *) session)->mem_lock);
}

int release_heap_lock(void *session)
{
   return release_lock(& ((biterc_session_t *) session)->mem_lock);
}

int take_queue_lock(void *session)
{
   return take_lock(& ((biterc_session_t *) session)->queue_lock);
}

int release_queue_lock(void *session)
{
   return release_lock(& ((biterc_session_t *) session)->queue_lock);
}

static int init_heap(biterc_session_t *session)
{
   int result;
   int pagesize = getpagesize();

   result = take_heap_lock(session);
   if (result == -1)
      return -1;

   init_sheep(((unsigned char *) session->shm) + pagesize, session->shm_size - pagesize, 0);

   result = release_heap_lock(session);
   if (result == -1)
      return -1;


   result = init_message(session->shared_header->num_procs, &session->shared_header->msg_table_ptr, session);
   if (result == -1) {
      return -1;
   }

   return 0;
}

static int biter_connect(const char *tmpdir, biterc_session_t *session)
{
   char c2s_path[MAX_PATH_LEN+1], s2c_path[MAX_PATH_LEN+1];
   int result, c2s_fd = -1, s2c_fd = -1, pipe_lock_held = 0;
   uint32_t local_id, all_connected, rank;
   struct stat buf;
   int unique_number;

   unique_number = biterc_get_unique_number(&session->shared_header->max_rank,
                                            &session->shared_header->num_set_max_rank,
                                            session);

   rank = biterc_get_rank(sessions - session);
   
   snprintf(c2s_path, MAX_PATH_LEN+1, "%s/biter_c2s.%d", tmpdir, unique_number);
   c2s_path[MAX_PATH_LEN] = '\0';

   snprintf(s2c_path, MAX_PATH_LEN+1, "%s/biter_s2c.%d", tmpdir, unique_number);
   s2c_path[MAX_PATH_LEN] = '\0';

   if (session->leader) {
      take_lock(&session->pipe_lock);
      pipe_lock_held = 1;

      while ((result = stat(c2s_path, &buf)) == -1 && errno == ENOENT)
         usleep(10000); //.01 sec
      if (result != 0) {
         biter_lasterror = "Could not stat client to server path";
         goto error;
      }

      while ((result = stat(s2c_path, &buf)) == -1 && errno == ENOENT)
         usleep(10000); //.01 sec
      if (result != 0) {
         biter_lasterror = "Could not stat server to client path";
         goto error;
      }

      session->shared_header->ready = 1;
   }
   else {
      while (session->shared_header->ready == 0)
         usleep(10000); //.01 sec

      take_lock(&session->pipe_lock);
      pipe_lock_held = 1;
   }

   c2s_fd = open(c2s_path, O_WRONLY);
   if (c2s_fd == -1) {
      biter_lasterror = "Unable to open client to server path";
      goto error;
   }

   s2c_fd = open(s2c_path, O_RDONLY);
   if (s2c_fd == -1) {
      biter_lasterror = "Unable to open server to client path";
      goto error;
   }

   local_id = (uint32_t) session->id;

   result = write(c2s_fd, &local_id, sizeof(local_id));
   if (result == -1) {
      biter_lasterror = "Unable to send initialization id to server";
      goto error;
   }

   result = write(c2s_fd, &rank, sizeof(rank));
   if (result == -1) {
      biter_lasterror = "Unable to send rank to server";
      goto error;
   }

   result = read(s2c_fd, &all_connected, sizeof(all_connected));
   if (result == -1) {
      biter_lasterror = "Unable to recieved connection message from server";
      goto error;
   }

   release_lock(&session->pipe_lock);
   pipe_lock_held = 0;

   if (all_connected) {
      session->shared_header->num_procs = all_connected;
      MEMORY_BARRIER;
      session->shared_header->ready = 2;
   }
   else {
      while (session->shared_header->ready != 2) {
         usleep(10000); //.01 sec
      }
   }

   session->c2s_fd = c2s_fd;
   session->s2c_fd = s2c_fd;

   return 0;

  error:
   if (pipe_lock_held)
      release_lock(&session->pipe_lock);
   if (s2c_fd != -1)
      close(s2c_fd);
   if (c2s_fd != -1)
      close(c2s_fd);

   return -1;
}

int biterc_newsession(const char *tmpdir, size_t shm_size)
{
   biterc_session_t *session = NULL;
   int biter_session;
   int result;

   if (biter_cur_session >= BITER_MAX_SESSIONS) {
      biter_lasterror = "Exceeded maximum number of sessions";
      goto error;
   }
   
   biter_session = biter_cur_session++;
   session = sessions + biter_session;

   result = init_shm(tmpdir, shm_size, session);
   if (result == -1)
      goto error;

   result = init_locks(session);
   if (result == -1)
      goto error;

   result = setup_ids(session);
   if (result == -1)
      goto error;

   result = biter_connect(tmpdir, session);
   if (result == -1)
      goto error;
   
   result = init_heap(session);
   if (result == -1)
      goto error;

   return biter_session;

  error:
   return -1;
}

int test_pipe_lock(void *session)
{
   return test_lock(& ((biterc_session_t *) session)->pipe_lock);
}

int take_pipe_lock(void *session)
{
   return take_lock(& ((biterc_session_t *) session)->pipe_lock);
}

int release_pipe_lock(void *session)
{
   return release_lock(& ((biterc_session_t *) session)->pipe_lock);
}

int take_write_lock(void *session)
{
   return take_lock(& ((biterc_session_t *) session)->write_lock);
}

int release_write_lock(void *session)
{
   return release_lock(& ((biterc_session_t *) session)->write_lock);
}

void set_last_error(const char *errstr)
{
   biter_lasterror = errstr;
}

int biterc_read(int biter_session, void *buf, size_t size)
{
   biterc_session_t *session = sessions + biter_session;
   return demultiplex_read(session, session->s2c_fd, session->id, buf, size);
}

int biterc_write(int biter_session, void *buf, size_t size)
{
   biterc_session_t *session = sessions + biter_session;
   return demultiplex_write(session, session->c2s_fd, session->id, buf, size);
}

const char *biterc_lasterror_str()
{
   return biter_lasterror;
}

int biterc_get_id(int biter_session)
{
   return sessions[biter_session].id;
}

void set_polled_data(void *session, msg_header_t msg)
{
   biterc_session_t *s = (biterc_session_t *) session;
   assert(!s->shared_header->has_polled_data);
   s->shared_header->polled_data = msg;
   s->shared_header->has_polled_data = 1;
}

void get_polled_data(void *session, msg_header_t *msg)
{
   biterc_session_t *s = (biterc_session_t *) session;
   assert(s->shared_header->has_polled_data);
   *msg = s->shared_header->polled_data;
}

int has_polled_data(void *session)
{
   biterc_session_t *s = (biterc_session_t *) session;
   return s->shared_header->has_polled_data;
}

void clear_polled_data(void *session)
{
   biterc_session_t *s = (biterc_session_t *) session;
   s->shared_header->has_polled_data = 0;
}

void set_heap_blocked(void *session)
{
   biterc_session_t *s = (biterc_session_t *) session;
   s->shared_header->heap_blocked = 1;
}

void set_heap_unblocked(void *session)
{
   biterc_session_t *s = (biterc_session_t *) session;
   s->shared_header->heap_blocked = 0;
}

int is_heap_blocked(void *session)
{
   biterc_session_t *s = (biterc_session_t *) session;
   return s->shared_header->heap_blocked;
}

int get_id(int session_id)
{
   biterc_session_t *session = sessions + session_id;
   return session->id;  
}

int is_client()
{
   return 1;
}
