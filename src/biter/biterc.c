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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <assert.h>

#include "biterc.h"
#include "sheep.h"
#include "demultiplex.h"
#include "ids.h"
#include "config.h"

#include "spindle_debug.h"
#include "shmutil.h"

#if !defined(MAX_PATH_LEN)
#define MAX_PATH_LEN 4096
#endif

#if !defined(BITER_MAX_SESSIONS)
#define BITER_MAX_SESSIONS 1
#endif

typedef struct biterc_session {
   shminfo_t *shm;
   lock_t pipe_lock;
   lock_t queue_lock;
   lock_t write_lock;
   int c2s_fd;
   int s2c_fd;
} biterc_session_t;

static biterc_session_t sessions[BITER_MAX_SESSIONS];
static int biter_cur_session = 0;

static const char *biter_lasterror = NULL;

extern int init_message(int num_procs, void *header_ptr, void *session);

int take_queue_lock(void *session)
{
   return take_lock(& ((biterc_session_t *) session)->queue_lock);
}

int release_queue_lock(void *session)
{
   return release_lock(& ((biterc_session_t *) session)->queue_lock);
}

struct entries_t {
   uint32_t max_rank;
   uint32_t num_ranks;
};

static int init_locks(biterc_session_t *session)
{
   shminfo_t *shm = session->shm;
   biter_header_t *header = &shm->shared_header->biter;
   session->pipe_lock.lock = header->locks + 0;
   session->pipe_lock.held_by = header->locks + 1;
   session->queue_lock.lock = header->locks + 2;
   session->queue_lock.held_by = header->locks + 3;
   session->write_lock.lock = header->locks + 4;
   session->write_lock.held_by = header->locks + 5;
   return init_heap_lock(shm);
}

static int setup_biter_ids(biterc_session_t *session)
{
   int result;
   result = setup_ids(session->shm);
   if (result == -1)
      return -1;

   return 0;
}

static int read_rank_file(const char *tmpdir, void *shared_page)
{
   int timeout = 6000; /* 60 seconds */
   int fd;
   ssize_t result, bytes_read = 0;
   char path[MAX_PATH_LEN+1];
   struct stat buf;

   snprintf(path, sizeof(path), "%s/rankFile", tmpdir);
   path[MAX_PATH_LEN] = '\0';

   /* Wait for file creation */
   for (;;) {
      result = stat(path, &buf);
      if (result == -1) {
         if (--timeout == 0) {
            err_printf("Failed to stat %s before timeout\n", path);
            return -1;
         }
         usleep(10000); /* .01 sec */
         continue;
      }
      else
         break;
   }

   assert(buf.st_size <= 2048); /*Max possible size: 256 nodes * 8 bytes */

   /* Open file */
   fd = open(path, O_RDONLY);
   if (fd == -1) {
      err_printf("Failed to open %s: %s\n", path, strerror(errno));
      return -1;
   }

   /* Read file */
   do {
      result = read(fd, ((char *) shared_page) + bytes_read, buf.st_size - bytes_read);
      if (result == -1) {
         err_printf("Failed to read from %s: %s\n", path, strerror(errno));
         close(fd);
         return -1;
      }
      bytes_read += result;
   } while (bytes_read < buf.st_size);
   debug_printf3("Read %u bytes from rankfile\n", (unsigned int) bytes_read);
      
   close(fd);
   
   return buf.st_size / 8;
}

static int biterc_get_unique_number(biterc_session_t *session, const char *tmpdir, void *shared_page)
{
   int result, i, num_entries;
   unsigned int myrank = biterc_get_rank(session - sessions);
   struct entries_t *entries = (struct entries_t *) shared_page;
   header_t *header = session->shm->shared_header;

   if (session->shm->leader) {
      debug_printf3("Leader reading rankfile\n");
      header->biter.read_file = read_rank_file(tmpdir, shared_page);
      debug_printf3("Leader read rankfile\n");
   }

   MEMORY_BARRIER;

   while (header->biter.read_file == 0);
   if (header->biter.read_file == -1) {
      err_printf("Leader process signaled error reading file.  Exiting\n");
      return -1;
   }

   if (header->biter.num_ranks == 0) {
      num_entries = header->biter.read_file;
      for (i = 0; i < num_entries; i++, entries++) {
         if (entries->max_rank == myrank) {
            header->biter.max_rank = myrank;
            header->biter.num_ranks = entries->num_ranks;
            debug_printf3("Found myrank, %d, in rankfile.  num_ranks is %d\n", header->biter.max_rank, header->biter.num_ranks);            break;
         }
      }
   }

   MEMORY_BARRIER;

   while (header->biter.num_ranks == 0);

   result = take_queue_lock(session);
   if (result == -1)
      return -1;

   if (header->biter.num_started+1 == header->biter.num_ranks) {
      debug_printf3("Last CN to read entries, clearing shared page\n");
      memset(shared_page, 0, num_entries * sizeof(struct entries_t));
   }

   header->biter.num_started++;

   result = release_queue_lock(session);
   if (result == -1)
      return -1;

   while (header->biter.num_ranks != header->biter.num_started);

   return header->biter.max_rank;
}

static int biter_connect(const char *tmpdir, biterc_session_t *session)
{
   char c2s_path[MAX_PATH_LEN+1], s2c_path[MAX_PATH_LEN+1];
   int result, c2s_fd = -1, s2c_fd = -1, pipe_lock_held = 0;
   uint32_t local_id, all_connected, rank;
   struct stat buf;
   int unique_number;
   shminfo_t *shm = session->shm;
   biter_header_t *biter_header = & shm->shared_header->biter;

#if defined(os_bluegene)
   unique_number = biterc_get_unique_number(session, tmpdir, shm->shared_header+1);
#else
   unique_number = 0;
   (void) biterc_get_unique_number;
#endif

   rank = biterc_get_rank(sessions - session);
   debug_printf3("biter rank = %u\n", rank);
   
   snprintf(c2s_path, MAX_PATH_LEN+1, "%s/biter_c2s.%d", tmpdir, unique_number);
   c2s_path[MAX_PATH_LEN] = '\0';

   snprintf(s2c_path, MAX_PATH_LEN+1, "%s/biter_s2c.%d", tmpdir, unique_number);
   s2c_path[MAX_PATH_LEN] = '\0'; 
   if (shm->leader) {
      take_lock(&session->pipe_lock);
      pipe_lock_held = 1;
      
      debug_printf3("biter test path %s\n", c2s_path);
      while ((result = stat(c2s_path, &buf)) == -1 && errno == ENOENT)
         usleep(10000); //.01 sec
      if (result != 0) {
         biter_lasterror = "Could not stat client to server path";
         goto error;
      }

      debug_printf3("biter test path %s\n", s2c_path);
      while ((result = stat(s2c_path, &buf)) == -1 && errno == ENOENT)
         usleep(10000); //.01 sec
      if (result != 0) {
         biter_lasterror = "Could not stat server to client path";
         goto error;
      }

      biter_header->ready = 1;
   }
   else {
      debug_printf3("biter non-master is waiting for ready\n");
      while (biter_header->ready == 0)
         usleep(10000); //.01 sec

      take_lock(&session->pipe_lock);
      pipe_lock_held = 1;
   }

   debug_printf3("Biter init opening %s\n", c2s_path);
   c2s_fd = open(c2s_path, O_WRONLY);
   if (c2s_fd == -1) {
      biter_lasterror = "Unable to open client to server path";
      goto error;
   }

   debug_printf3("Biter init opening %s\n", s2c_path);
   s2c_fd = open(s2c_path, O_RDONLY);
   if (s2c_fd == -1) {
      biter_lasterror = "Unable to open server to client path";
      goto error;
   }

   local_id = (uint32_t) shm->id;

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
      biter_header->num_procs = all_connected;
      MEMORY_BARRIER;
      biter_header->ready = 2;
   }
   else {
      debug_printf3("Waiting for all clients to finish connecting to %s\n", c2s_path);
      while (biter_header->ready != 2) {
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

   debug_printf3("Initializing biterc shm to %lu\n", (unsigned long) shm_size);
   result = init_shm(tmpdir, shm_size, biterc_get_job_id(), &session->shm);
   if (result == -1)
      goto error;

   debug_printf3("Initializing biterc locks\n");
   result = init_locks(session);
   if (result == -1)
      goto error;

   debug_printf3("Setting up biterc ids\n");
   result = setup_biter_ids(session);
   if (result == -1)
      goto error;

   debug_printf3("Connecting biterc to daemon via %s\n", tmpdir);
   result = biter_connect(tmpdir, session);
   if (result == -1)
      goto error;
   
   debug_printf3("Initializing biterc shared heap\n");
   result = init_heap(session->shm);
   if (result == -1)
      goto error;

   debug_printf3("Initializing message system\n");   
   result = init_message(session->shm->shared_header->biter.num_procs, 
                         &session->shm->shared_header->biter.msg_table_ptr, session);
   if (result == -1) {
      return -1;
   }

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
   err_printf("biter error: %s\n", errstr);
   biter_lasterror = errstr;
}

int biterc_read(int biter_session, void *buf, size_t size)
{
   biterc_session_t *session = sessions + biter_session;
   return demultiplex_read(session, session->s2c_fd, session->shm->id, buf, size);
}

int biterc_write(int biter_session, void *buf, size_t size)
{
   biterc_session_t *session = sessions + biter_session;
   return demultiplex_write(session, session->c2s_fd, session->shm->id, buf, size);
}

const char *biterc_lasterror_str()
{
   return biter_lasterror;
}

int biterc_get_id(int biter_session)
{
   return sessions[biter_session].shm->id;
}

int biterc_is_client_fd(int biter_session, int fd)
{
   return ((sessions[biter_session].shm->fd == fd) ||
           (sessions[biter_session].c2s_fd == fd) ||
           (sessions[biter_session].s2c_fd == fd));
}

void set_polled_data(void *session, msg_header_t msg)
{
   biterc_session_t *s = (biterc_session_t *) session;
   biter_header_t *header = &s->shm->shared_header->biter;
   assert(!header->has_polled_data);
   header->polled_data = msg;
   header->has_polled_data = 1;
}

void get_polled_data(void *session, msg_header_t *msg)
{
   biterc_session_t *s = (biterc_session_t *) session;
   biter_header_t *header = &s->shm->shared_header->biter;
   assert(header->has_polled_data);
   *msg = header->polled_data;
}

int has_polled_data(void *session)
{
   biterc_session_t *s = (biterc_session_t *) session;
   biter_header_t *header = &s->shm->shared_header->biter;
   return header->has_polled_data;
}

void clear_polled_data(void *session)
{
   biterc_session_t *s = (biterc_session_t *) session;
   biter_header_t *header = &s->shm->shared_header->biter;
   header->has_polled_data = 0;
}

void set_heap_blocked(void *session)
{
   biterc_session_t *s = (biterc_session_t *) session;
   biter_header_t *header = &s->shm->shared_header->biter;
   header->heap_blocked = 1;
}

void set_heap_unblocked(void *session)
{
   biterc_session_t *s = (biterc_session_t *) session;
   biter_header_t *header = &s->shm->shared_header->biter;
   header->heap_blocked = 0;
}

int is_heap_blocked(void *session)
{
   biterc_session_t *s = (biterc_session_t *) session;
   biter_header_t *header = &s->shm->shared_header->biter;
   return header->heap_blocked;
}

int get_id(int session_id)
{
   biterc_session_t *session = sessions + session_id;
   return session->shm->id;
}

int is_client()
{
   return 1;
}
