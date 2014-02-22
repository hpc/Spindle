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

#include "demultiplex.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct message_t {
   uint32_t size;
   uint32_t bytes_read;
   void *data;
   struct message_t *next_message;
} message_t;

#define PROC_MESSAGES ((message_t **) get_proc_messages(session))

extern int mark_bytes_cached(unsigned long bytec, void *session);
extern int clear_bytes_cached(unsigned long bytec, void *session);

extern void *get_proc_messages(void *session);
void set_proc_messages(void *session, void *new_proc_messages);

void init_queue(int num_procs, void *session)
{
   void *proc_messages;

   proc_messages = malloc(sizeof(message_t *) * num_procs);
   memset(proc_messages, 0, sizeof(message_t *) * num_procs);
   set_proc_messages(session, proc_messages);
}

int test_pipe_lock(void *session)
{
   return 1;
}

int release_pipe_lock(void *session)
{
   return 0;
}

int take_queue_lock(void *session)
{
   return 0;
}

int release_queue_lock(void *session)
{
   return 0;
}

int take_write_lock(void *session)
{
   return 0;
}

int release_write_lock(void *session)
{
   return 0;
}

int take_pipe_lock(void *session)
{
   return 0;
}

void get_message(int for_proc, void **msg_data, size_t *msg_size, size_t *bytes_read, void *session)
{
   message_t *msg = PROC_MESSAGES[for_proc];
   assert(msg);

   *msg_data = msg->data;
   *msg_size = msg->size;
   *bytes_read = msg->bytes_read;
}

int has_message(int for_proc, void *session)
{
   return PROC_MESSAGES[for_proc] != NULL;
}

void rm_message(int for_proc, void *session)
{
   message_t *msg = PROC_MESSAGES[for_proc];
   PROC_MESSAGES[for_proc] = msg->next_message;

   clear_bytes_cached(msg->size, session);

   free(msg->data);
   free(msg);
}

int enqueue_message(int for_proc, void *msg_data, size_t msg_size, void *header_space, void *session)
{
   message_t *new_msg = (message_t *) header_space;
   message_t **cur;

   new_msg->size = msg_size;
   new_msg->bytes_read = 0;
   new_msg->data = msg_data;
   new_msg->next_message = NULL;

   for (cur = PROC_MESSAGES+for_proc; *cur != NULL; cur = & (*cur)->next_message);
   *cur = new_msg;

   mark_bytes_cached(msg_size, session);

   return 0;
}

int get_message_space(size_t msg_size, unsigned char **msg_space, void **header_space, void *session)
{
   *msg_space = (unsigned char *) malloc(msg_size);
   *header_space = malloc(sizeof(message_t));
   return 0;
}

void update_bytes_read(int for_proc, size_t newval, void *session)
{
   PROC_MESSAGES[for_proc]->bytes_read = newval;
}

