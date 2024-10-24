#include "msgbundle.h"
#include "spindle_debug.h"
#include "ldcs_audit_server_process.h"
#include "ldcs_audit_server_md.h"
#include "ldcs_api_listen.h"
#include "spindle_launch.h"

#include <assert.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

static pthread_t thrd_handle;
static pthread_mutex_t mut;
static pthread_cond_t timeout_sync;
static volatile int active_timeout, active_flush;
static volatile int done = 0;
static int flush_pipe[2];
static int initialized = 0;

static int flush_msgbuffer_cb(int fd, int serverid, void *data);
static void *thrd_main(void *timeout);
int spindle_send_worker(ldcs_process_data_t *procdata, ldcs_message_t *msg, node_peer_t node,
                        void *secondary_data, size_t secondary_size);

#define BROADCAST ((node_peer_t) (long) -2)
#define PARENT ((node_peer_t) (long) -3)
#define PASSTHROUGH -3

void msgbundle_init(ldcs_process_data_t *procdata)
{
   if (!(procdata->opts & OPT_MSGBUNDLE)) {
      debug_printf("Message bundling disabled\n");
      return;
   }

   assert(procdata->msgbundle_cache_size_kb);
   assert(procdata->msgbundle_timeout_ms);
   procdata->msgbundle_entries = NULL;
   debug_printf("Initializing message bundling with buffer of size %u kb and "
                "send timeout of %u ms\n", procdata->msgbundle_cache_size_kb,
                procdata->msgbundle_timeout_ms);

   debug_printf2("Spawning timeout monitoring thread for cache buffers\n");
   (void)! pipe(flush_pipe);
   ldcs_listen_register_fd(flush_pipe[0], procdata->serverid, &flush_msgbuffer_cb, (void *) procdata);
   pthread_mutex_init(&mut, NULL);
   pthread_cond_init(&timeout_sync, NULL);
   pthread_create(&thrd_handle, NULL, thrd_main, (void *) (long) procdata->msgbundle_timeout_ms);

   initialized = 1;
}

void msgbundle_done(ldcs_process_data_t *procdata)
{   
   void *retval;
   if (!initialized)
      return;
   
   pthread_mutex_lock(&mut);
   done = 1;
   pthread_cond_broadcast(&timeout_sync);
   pthread_mutex_unlock(&mut);

   ldcs_listen_unregister_fd(flush_pipe[0]);
   close(flush_pipe[0]);
   close(flush_pipe[1]);
   initialized = 0;
   pthread_join(thrd_handle, &retval);
   pthread_cond_destroy(&timeout_sync);
   pthread_mutex_destroy(&mut);
}

int spindle_send_noncontig(ldcs_process_data_t *procdata, ldcs_message_t *msg, node_peer_t node,
                           void *secondary_data, size_t secondary_size)
{
   int result = spindle_send_worker(procdata, msg, node, secondary_data, secondary_size);
   if (result == PASSTHROUGH)
      return ldcs_audit_server_md_send_noncontig(procdata, msg, node, secondary_data, secondary_size);
   return result;
}

int spindle_send(ldcs_process_data_t *procdata, ldcs_message_t *msg, node_peer_t node)
{
   int result = spindle_send_worker(procdata, msg, node, NULL, 0);
   if (result == PASSTHROUGH)
      return ldcs_audit_server_md_send(procdata, msg, node);
   return result;
}

int spindle_broadcast_noncontig(ldcs_process_data_t *procdata, ldcs_message_t *msg,
                                void *secondary_data, size_t secondary_size)
{
   int result = spindle_send_worker(procdata, msg, BROADCAST, secondary_data, secondary_size);
   if (result == PASSTHROUGH)
      return ldcs_audit_server_md_broadcast_noncontig(procdata, msg, secondary_data, secondary_size);
   return result;
}

int spindle_broadcast(ldcs_process_data_t *procdata, ldcs_message_t *msg)
{
   int result = spindle_send_worker(procdata, msg, BROADCAST, NULL, 0);
   if (result == PASSTHROUGH)
      return ldcs_audit_server_md_broadcast(procdata, msg);
   return result;
}

int spindle_forward_query(ldcs_process_data_t *procdata, ldcs_message_t *msg)
{
   int result = spindle_send_worker(procdata, msg, PARENT, NULL, 0);
   if (result == PASSTHROUGH)
      return ldcs_audit_server_md_forward_query(procdata, msg);
   return result;
}


static void *thrd_main(void *timeout)
{
   int timeout_ms = (int) (long) timeout;
   int result;

   for (;;) {
      pthread_mutex_lock(&mut);
      while (!active_timeout && !done)
         pthread_cond_wait(&timeout_sync, &mut);      
      pthread_mutex_unlock(&mut);

      if (done)
         return NULL;

      debug_printf2("Starting message buffer cache timeout of %d ms.\n", timeout_ms);      
      usleep(timeout_ms * 1000);
      do {
         debug_printf2("Triggering message buffer cache flush due to timeout\n");
         result = write(flush_pipe[1], "f", 1);
      } while (result == EINTR);

      pthread_mutex_lock(&mut);
      active_timeout = 0;
      active_flush = 1;
      while (active_flush && !done)
         pthread_cond_wait(&timeout_sync, &mut);      
      pthread_mutex_unlock(&mut);

      if (done)
         return NULL;
   }
}

static int flush_msgbuffer(msgbundle_entry_t *mb, ldcs_process_data_t *procdata)
{
   ldcs_message_t msg;
   int result;

   if (!mb->position)
      return 0;

   msg.header.type = LDCS_MSG_BUNDLE;
   msg.header.len = mb->position;
   msg.data = (char *) mb->cache;

   debug_printf2("Flushing message buffer for node %s\n", mb->name);
   if (mb->node == BROADCAST)
      result = ldcs_audit_server_md_broadcast(procdata, &msg);
   else if (mb->node == PARENT)
      result = ldcs_audit_server_md_forward_query(procdata, &msg);
   else
      result = ldcs_audit_server_md_send(procdata, &msg, mb->node);
   mb->position = 0;
   return result;
}

static int flush_msgbuffer_cb(int fd, int serverid, void *data)
{
   msgbundle_entry_t *mb;
   ldcs_process_data_t *procdata = (ldcs_process_data_t *) data;
   char throwaway_byte;

   if (fd != -1)
      (void)! read(fd, (void *) &throwaway_byte, 1);
   
   for (mb = procdata->msgbundle_entries; mb; mb = mb->next) {
      debug_printf3("flushing for mb %s\n", mb->name);
      flush_msgbuffer(mb, procdata);
   }

   pthread_mutex_lock(&mut);
   active_flush = 0;
   pthread_cond_broadcast(&timeout_sync);
   pthread_mutex_unlock(&mut);
   return 0;
}

void msgbundle_force_flush(ldcs_process_data_t *procdata)
{
   if (!procdata || !(procdata->opts & OPT_MSGBUNDLE))
      return;
   flush_msgbuffer_cb(-1, 0, procdata);
}

static void start_cache_timeout(int timeout_ms, ldcs_process_data_t *procdata)
{
   pthread_mutex_lock(&mut);

   if (!active_timeout) {
      active_timeout = 1;
      pthread_cond_broadcast(&timeout_sync);
   }
   
   pthread_mutex_unlock(&mut);
}

int spindle_send_worker(ldcs_process_data_t *procdata, ldcs_message_t *msg, node_peer_t node,
                        void *secondary_data, size_t secondary_size)
{
   msgbundle_entry_t *mb;

   if (!procdata || !(procdata->opts & OPT_MSGBUNDLE))
      return PASSTHROUGH;

   if (ldcs_audit_server_md_get_num_children(procdata) == 0 && node == BROADCAST) {
      debug_printf3("Shortcircuiting broadcast because I am a leaf node\n");
      return 0;
   }
   
   debug_printf2("Processing message of size header:%lu + body:%lu (secondary:%lu) = %lu for message bundling\n",
                 sizeof(ldcs_message_header_t), (unsigned long) msg->header.len, secondary_size,
                 sizeof(ldcs_message_header_t) + msg->header.len);

   for (mb = procdata->msgbundle_entries; mb && mb->node != node; mb = mb->next);

   if (sizeof(msg->header)*2 + msg->header.len >=
       procdata->msgbundle_cache_size_kb*1024)
   {
      debug_printf2("Not using message bundling because packet size (%ld) is greater than"
                    "cache size %d.\n",
                    sizeof(msg->header)*2 + msg->header.len,
                    procdata->msgbundle_cache_size_kb*1024);
      if (mb)
         flush_msgbuffer(mb, procdata);
      return PASSTHROUGH;
   }

   if (!mb) {
      debug_printf2("Creating new message bundle cache for node %p\n", node);
      mb = (msgbundle_entry_t *) malloc(sizeof(msgbundle_entry_t));
      mb->cache = (unsigned char *) malloc(procdata->msgbundle_cache_size_kb*1024);
      mb->position = 0;
      mb->node = node;
      mb->next = procdata->msgbundle_entries;
      if (mb->node == BROADCAST)
         snprintf(mb->name, sizeof(mb->name), "BROADCAST");
      else if (mb->node == PARENT)
         snprintf(mb->name, sizeof(mb->name), "PARENT");
      else
         snprintf(mb->name, sizeof(mb->name), "%lu", (unsigned long) mb->node);
      mb->name[sizeof(mb->name)-1] = '\0';
      procdata->msgbundle_entries = mb;
   }
   else {
      debug_printf2("Selected existing message bundle for node %s with size %lu/%lu\n",
                    mb->name, (unsigned long) mb->position,
                    (unsigned long) procdata->msgbundle_cache_size_kb*1024);
   }
   

   if (sizeof(msg->header) + msg->header.len + mb->position >=
       procdata->msgbundle_cache_size_kb*1024)
   {
      debug_printf2("Flushing message buffer due to no space for adding new message."
                    "Current size = %u, new message size = %lu, capacity = %u\n",
                    mb->position,
                    sizeof(msg->header) + msg->header.len,
                    procdata->msgbundle_cache_size_kb*1024);
      flush_msgbuffer(mb, procdata);
   }

   if (!mb->position) {
      debug_printf2("Starting new message buffer\n");
   }
   else {
      debug_printf2("Appending message at position %d\n", mb->position);
   }

   memcpy(mb->cache + mb->position, &msg->header, sizeof(ldcs_message_header_t));
   mb->position += sizeof(ldcs_message_header_t);
   memcpy(mb->cache + mb->position, msg->data, (msg->header.len - secondary_size));
   mb->position += (msg->header.len - secondary_size);
   if (secondary_size) {
      memcpy(mb->cache + mb->position, secondary_data, secondary_size);
      mb->position += secondary_size;
   }
   debug_printf2("Cached data in message buffer to node %s, which is %u of %u bytes full.\n",
                 mb->name, (int) mb->position, procdata->msgbundle_cache_size_kb*1024);

   start_cache_timeout(procdata->msgbundle_timeout_ms, procdata);
   return 0;
}
