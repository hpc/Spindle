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
#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>

#include "biterd.h"
#include "demultiplex.h"
#include "daemon_ids.h"
#include "spindle_debug.h"

static const char *biter_lasterror = NULL;

#if !defined(BITER_MAX_SESSIONS)
#define BITER_MAX_SESSIONS 256
#endif

#define FD_SESSION(X) (((X) & 0xffff0000) >> 16)
#define FD_CLIENT(X) (X & 0x0000ffff)

typedef struct biterd_session {
   int c2s_fd;
   int s2c_fd;
   char *c2s_path;
   char *s2c_path;
   int num_clients;
   int clients_accepted;
   void *proc_messages;
   msg_header_t polled_data;
   int has_polled_data;
   int heap_blocked;
   int last_cached_data_avail;
   unsigned long local_bytes_cached;
} biterd_session_t;

static biterd_session_t sessions[BITER_MAX_SESSIONS];
static int max_session = 0;

extern void init_queue(int num_procs, void *session);

void *get_proc_messages(void *session)
{
   return ((biterd_session_t *) session)->proc_messages;
}

void set_proc_messages(void *session, void *new_proc_messages)
{
   ((biterd_session_t *) session)->proc_messages = new_proc_messages;
}

static int biterd_accept(int session_id)
{
   int result;
   uint32_t client_id, all_done, rank;
   biterd_session_t *session = sessions + session_id;

   result = read(session->c2s_fd, &client_id, sizeof(client_id));
   if (result == -1) {
      biter_lasterror = "Unable to read client connection message";
      return -1;
   }

   result = read(session->c2s_fd, &rank, sizeof(rank));
   if (result == -1) {
      biter_lasterror = "Unable to read client connection message";
      return -1;
   }

   if (client_id > session->num_clients) {
      biter_lasterror = "Recieved invalid client id during connection";
      return -1;
   }

   biterd_register_rank(session_id, client_id, rank);

   session->clients_accepted++;
   all_done = (session->clients_accepted == session->num_clients) ? session->num_clients : 0;
   
   result = write(session->s2c_fd, &all_done, sizeof(all_done));
   if (result == -1) {
      biter_lasterror = "Unable to write client connection message";
      return -1;
   }

   return 0;
}

void set_last_error(const char *errstr)
{
   err_printf("biter error: %s\n", errstr);
   biter_lasterror = errstr;
}

int biterd_newsession(const char *tmpdir, int cn_id)
{
   char *c2s_path = NULL, *s2c_path = NULL;
   int result, c2s_fd = -1, s2c_fd = -1;
   int path_len = strlen(tmpdir) + 32;
   int session_id, i, unique_number, num_clients;

   assert(sizeof(int) == sizeof(uint32_t)); //Fix FD_* macros if this fails on new platform

   biterd_init_comms(tmpdir);

   if (cn_id > BITER_MAX_SESSIONS || cn_id > biterd_num_compute_nodes()) {
      biter_lasterror = "Out of biter session IDs";
      goto error;
   }
   
   unique_number = biterd_unique_num_for_cn(cn_id);
   num_clients = biterd_ranks_in_cn(cn_id);

   //Create the client to server communication path
   c2s_path = (char *) malloc(path_len);   
   if (!c2s_path) {
      biter_lasterror = "Unable to allocate memory for client to server path\n";
      goto error;
   }
   snprintf(c2s_path, path_len, "%s/biter_c2s.%d", tmpdir, unique_number);
   c2s_path[path_len-1] = '\0';

   result = mkfifo(c2s_path, 0600);
   if (result == -1) {
      *c2s_path = '\0';
      biter_lasterror = "Unable to mkfifo for the client to server pipe";
      goto error;
   }

   //Create the server to client communication path
   s2c_path = (char *) malloc(path_len);   
   if (!s2c_path) {
      biter_lasterror = "Unable to allocate memory for server to client path\n";
      goto error;
   }
   snprintf(s2c_path, path_len, "%s/biter_s2c.%d", tmpdir, unique_number);
   s2c_path[path_len-1] = '\0';

   result = mkfifo(s2c_path, 0600);
   if (result == -1) {
      *s2c_path = '\0';
      biter_lasterror = "Unable to mkfifo for the server to client pipe";
      goto error;
   }

   //Open both pipes
   c2s_fd = open(c2s_path, O_RDONLY);
   if (c2s_fd == -1) {
      biter_lasterror = "Unable to open the client to server pipe";
      goto error;
   }

   s2c_fd = open(s2c_path, O_WRONLY);
   if (s2c_fd == -1) {
      biter_lasterror = "Unable to open the server to client pipe";
      goto error;
   }

   session_id = cn_id;
   assert(session_id < BITER_MAX_SESSIONS);

   sessions[session_id].c2s_fd = c2s_fd;
   sessions[session_id].s2c_fd = s2c_fd;
   sessions[session_id].c2s_path = c2s_path;
   sessions[session_id].s2c_path = s2c_path;
   sessions[session_id].num_clients = num_clients;
   sessions[session_id].clients_accepted = 0;
   sessions[session_id].polled_data.msg_size = 0;
   sessions[session_id].polled_data.msg_target = 0;
   sessions[session_id].has_polled_data = 0;
   sessions[session_id].heap_blocked = 0;

   init_queue(num_clients, sessions+session_id);

   for (i = 0; i < num_clients; i++) {
      biterd_accept(session_id);
   }
   
   if (session_id >= max_session)
      max_session = session_id+1;

   return session_id;

  error:
   if (c2s_path && *c2s_path != '\0')
      unlink(c2s_path);
   if (s2c_path && *s2c_path != '\0')
      unlink(s2c_path);
   if (c2s_path)
      free(c2s_path);
   if (s2c_path)
      free(s2c_path);
   if (c2s_fd != -1)
      close(c2s_fd);
   if (s2c_fd != -1)
      close(s2c_fd);
   return -1;
}

int biterd_num_clients(int session_id)
{
   return sessions[session_id].num_clients;
}

int biterd_read_fd(int session_id)
{
   return sessions[session_id].c2s_fd;
}

int biterd_write_fd(int session_id)
{
   return sessions[session_id].s2c_fd;
}

int biterd_find_client_w_data(int session_id)
{
   int result, client_id;
   biterd_session_t *session = sessions + session_id;
   result = demultiplex_poll(session, session->c2s_fd, &client_id);
   if (result == -1)
      return -1;
   return client_id;
}

int biterd_write(int session_id, int client_id, const void *buf, size_t count)
{
   biterd_session_t *session = sessions + session_id;
   return demultiplex_write(session, session->s2c_fd, client_id, buf, count);
}

int biterd_read(int session_id, int client_id, void *buf, size_t count)
{
   biterd_session_t *session = sessions + session_id;
   return demultiplex_read(session, session->c2s_fd, client_id, buf, count);
}

int biterd_clean(int session_id)
{
   biterd_session_t *session;
   if (session_id < 0 || session_id > biterd_num_compute_nodes()) {
      biter_lasterror = "Invalid session id";
      return -1;
   }

   session = sessions + session_id;

   if (session->c2s_fd != -1) {
      close(session->c2s_fd);
      session->c2s_fd = -1;
   }
   if (session->s2c_fd != -1) {
      close(session->s2c_fd);
      session->s2c_fd = -1;
   }
   if (session->c2s_path) {
      unlink(session->c2s_path);
      free(session->c2s_path);
      session->c2s_path = NULL;
   }
   if (session->s2c_path) {
      unlink(session->s2c_path);
      free(session->s2c_path);
      session->s2c_path = NULL;
   }
   session->num_clients = 0;
   session->clients_accepted = 0;
   return 0;
}


const char *biterd_lasterror_str()
{
   return biter_lasterror;
}

void set_polled_data(void *session, msg_header_t msg)
{
   biterd_session_t *s = (biterd_session_t *) session;
   assert(!s->has_polled_data);
   s->polled_data = msg;
   s->has_polled_data = 1;
}

void get_polled_data(void *session, msg_header_t *msg)
{
   biterd_session_t *s = (biterd_session_t *) session;
   assert(s->has_polled_data);
   *msg = s->polled_data;
}

int has_polled_data(void *session)
{
   biterd_session_t *s = (biterd_session_t *) session;
   return s->has_polled_data;
}

void clear_polled_data(void *session)
{
   biterd_session_t *s = (biterd_session_t *) session;
   s->has_polled_data = 0;
}

void set_heap_blocked(void *session)
{
   biterd_session_t *s = (biterd_session_t *) session;
   s->heap_blocked = 1;
}

void set_heap_unblocked(void *session)
{
   biterd_session_t *s = (biterd_session_t *) session;
   s->heap_blocked = 0;
}

int is_heap_blocked(void *session)
{
   biterd_session_t *s = (biterd_session_t *) session;
   return s->heap_blocked;
}


static int r_aux_fd = -1;
static int w_aux_fd = -1;
static unsigned long bytes_cached = 0;

static int get_aux_fd()
{
   int result;
   int fds[2];

   if (r_aux_fd == -1) {
      result = pipe(fds);
      if (result != 0)
         return -1;
      r_aux_fd = fds[0];
      w_aux_fd = fds[1];
   }

   return r_aux_fd;
}

int biterd_fill_in_read_set(int session_id, fd_set *readset, int *max_fd)
{
   int fd = sessions[session_id].c2s_fd;
   int aux_fd = get_aux_fd();
   
   FD_SET(fd, readset);
   if (fd > *max_fd)
      *max_fd = fd;

   if (bytes_cached && !FD_ISSET(aux_fd, readset)) {
      FD_SET(aux_fd, readset);
      if (aux_fd > *max_fd)
         *max_fd = aux_fd;
   }

   return 0;
}

int biterd_get_fd(int session_id)
{
   return sessions[session_id].c2s_fd;
}

int biterd_get_aux_fd()
{
   return get_aux_fd();
}

int biterd_has_data_avail(int session_id, fd_set *readset)
{
   biterd_session_t *session = sessions + session_id;
   int client_id;
   int fd = sessions[session_id].c2s_fd;
   int aux_fd = get_aux_fd();
   int i, orig;

   session = sessions + session_id;

   if (FD_ISSET(aux_fd, readset) && session->local_bytes_cached) {
      i = orig = session->last_cached_data_avail;
      for (;;) {
         if (i >= session->num_clients)
            i = 0;
         if (has_message(i, session)) {
            session->last_cached_data_avail = i+1;
            return i;
         }
         i++;
         assert(i != orig);
      }
   }

   if (FD_ISSET(fd, readset)) {
      client_id = biterd_find_client_w_data(session_id);
      if (client_id != -1)
         return client_id;
   }
   
   return -1;
}

int biterd_get_session_proc_w_aux_data(int *session_result, int *proc_result)
{
   static int last_session = 0;
   int cur_session, start_session, j;
   biterd_session_t *session;
   cur_session = start_session = last_session;

   do {
      session = sessions+cur_session;

      if (session->local_bytes_cached) {
         for (j = 0; j < session->num_clients; j++) {
            if (has_message(j, session)) {
               *session_result = cur_session;
               *proc_result = j;

               last_session = cur_session+1;
               if (last_session == max_session)
                  last_session = 0;

               return 0;
            }
         }
      }

      cur_session++;
      if (cur_session == max_session)
         cur_session = 0;      
   } while (cur_session != start_session);
   
   return -1;
}

int mark_bytes_cached(unsigned long bytec, void *session)
{
   char a_byte = 'b';
   biterd_session_t *s = (biterd_session_t *) session;
   get_aux_fd();

   if (bytec == 0)
      return 0;

   if (bytes_cached == 0) {
      (void)! write(w_aux_fd, &a_byte, 1);
   }
   bytes_cached += bytec;
   s->local_bytes_cached += bytec;
   return 0;
}

int clear_bytes_cached(unsigned long bytec, void *session)
{
   char a_byte;
   biterd_session_t *s = (biterd_session_t *) session;
   get_aux_fd();

   assert(bytec <= bytes_cached);
   assert(bytec <= s->local_bytes_cached);

   bytes_cached -= bytec;
   s->local_bytes_cached -= bytec;

   if (bytes_cached == 0) {
      (void)! read(r_aux_fd, &a_byte, 1);
   }
   
   return 0;
}

int is_client()
{
   return 0;
}
