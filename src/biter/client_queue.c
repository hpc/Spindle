#include "demultiplex.h"
#include "sheep.h"

#include <string.h>
#include <assert.h>

typedef struct message_t {
   uint32_t size;
   uint32_t bytes_read;
   sheep_ptr_t data;
   sheep_ptr_t next_message;
} message_t;

extern int take_heap_lock(void *session);
extern int release_heap_lock(void *session);
extern int take_queue_lock(void *session);
extern int release_queue_lock(void *session);

static sheep_ptr_t *proc_messages;

int init_message(int num_procs, void *header_ptr, void *session)
{
   sheep_ptr_t *sp;
   int result, i;
   int queue_lock_held = 0, heap_lock_held = 0;

   sp = ((sheep_ptr_t *) header_ptr);

   result = take_queue_lock(session);
   if (result == -1)
      goto error;
   queue_lock_held = 1;

   if (IS_SHEEP_NULL(sp)) {
      //No one has initialized the header yet.  Do so.
      result = take_heap_lock(session);
      if (result == -1)
         goto error;
      heap_lock_held = 1;
      
      proc_messages = malloc_sheep(sizeof(sheep_ptr_t) * num_procs);
      
      if (!proc_messages)
         goto error;

      result = release_heap_lock(session);
      if (result == -1)
         goto error;
      heap_lock_held = 0;
      
      for (i = 0; i < num_procs; i++) {
         set_sheep_ptr(proc_messages+i, SHEEP_NULL);
      }
      set_sheep_ptr(sp, proc_messages);
   }
   else {
      proc_messages = sheep_ptr(sp);
   }

   result = release_queue_lock(session);
   if (result == -1) 
      goto error;

   return 0;

  error:

   if (heap_lock_held)
      release_heap_lock(session);
   if (queue_lock_held)
      release_queue_lock(session);

   return -1;   
}

void get_message(int for_proc, void **msg_data, size_t *msg_size, size_t *bytes_read, void *session)
{
   message_t *msg;
   sheep_ptr_t *msgp = proc_messages + for_proc;
   assert(!IS_SHEEP_NULL(msgp));
   msg = (message_t *) sheep_ptr(msgp);

   *msg_data = sheep_ptr(& msg->data);
   *msg_size = msg->size;
   *bytes_read = msg->bytes_read;
}

int has_message(int for_proc, void *session)
{
   sheep_ptr_t *msgp = proc_messages + for_proc;
   return !IS_SHEEP_NULL(msgp);
}

void rm_message(int for_proc, void *session)
{
   int result;

   message_t *msg;
   sheep_ptr_t *msgp = proc_messages + for_proc;
   assert(!IS_SHEEP_NULL(msgp));
   msg = (message_t *) sheep_ptr(msgp);

   *msgp = msg->next_message;
   
   result = take_heap_lock(session);
   if (result == -1)
      return;
   
   free_sheep(sheep_ptr(&msg->data));
   free_sheep(msg);

   release_heap_lock(session);

   set_heap_unblocked(session);
}

int enqueue_message(int for_proc, void *msg_data, size_t msg_size, void *header_space, void *session)
{
   sheep_ptr_t *queue = proc_messages+for_proc;
   sheep_ptr_t *cur;
   message_t *new_msg;
   int result, queue_lock_held = 0;

   new_msg = (message_t *) header_space;
   new_msg->size = (uint32_t) msg_size;
   new_msg->bytes_read = 0;
   new_msg->data = ptr_sheep(msg_data);
   new_msg->next_message = ptr_sheep(SHEEP_NULL);
   
   result = take_queue_lock(session);
   if (result == -1)
      goto error;
   queue_lock_held = 1;

   for (cur = queue; !IS_SHEEP_NULL(cur); cur = &((message_t *) sheep_ptr(cur))->next_message);
   *cur = ptr_sheep(new_msg);
   
   result = release_queue_lock(session);
   if (result == -1)
      goto error;
   queue_lock_held = 0;

   return 0;

  error:

   if (queue_lock_held)
      release_queue_lock(session);

   return -1;
}

int get_message_space(size_t msg_size, unsigned char **msg_space, void **header_space, void *session)
{
   int have_heap_lock = 0, result;

   *msg_space = NULL;
   *header_space = NULL;

   result = take_heap_lock(session);
   if (result == -1)
      goto error;
   have_heap_lock = 1;

   *msg_space = (unsigned char *) malloc_sheep(msg_size);
   if (!*msg_space) 
      goto error;

   *header_space = malloc_sheep(sizeof(message_t));
   if (!*header_space)
      goto error;

   result = release_heap_lock(session);
   if (result == -1)
      goto error;
   have_heap_lock = 0;

   return 0;

  error:
   if (*msg_space)
      free_sheep(*msg_space);
   if (*header_space)
      free_sheep(*header_space);
   *msg_space = NULL;
   *header_space = NULL;

   if (have_heap_lock)
      release_heap_lock(session);

   return -1;
}

void update_bytes_read(int for_proc, size_t newval, void *session)
{
   message_t *msg;
   sheep_ptr_t *msgp = proc_messages + for_proc;
   assert(!IS_SHEEP_NULL(msgp));
   msg = (message_t *) sheep_ptr(msgp);

   msg->bytes_read = newval;
}
