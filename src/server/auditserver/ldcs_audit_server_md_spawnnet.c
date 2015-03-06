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

#include "ldcs_audit_server_md.h"
#include "spindle_debug.h"

#include "pmi.h"   // use PMI to exchange spawnnet endpoint addresses
#include "spawn.h"

#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netdb.h>

static int waitfor_spawnnet();

static int read_pipe;
static int write_pipe;
static int sockfd;
static pthread_t thrd;

static size_t num_children = 0;
static spawn_net_channel **children = NULL;
static spawn_net_channel *parent = SPAWN_NET_CHANNEL_NULL;
static lwgrp* group = NULL;

static int rank  = -1;
static int ranks =  0;

static int write_msg(spawn_net_channel *channel, ldcs_message_t *msg)
{
   int result;
   result = spawn_net_write(channel, msg, sizeof(*msg));
   if (result != 0) {
      err_printf("Error writing spawnnet header\n");
      return -1;
   }

   if (msg->header.len) {
       result = spawn_net_write(channel, msg->data, msg->header.len);
       if (result != 0) {
          err_printf("Error writing spawnnet body\n");
          return -1;
       }
   }

   return 0;
}

static int read_msg(spawn_net_channel *channel, ldcs_message_t *msg)
{
   int result;
   char *buffer = NULL;

   result = spawn_net_read(channel, msg, sizeof(*msg));
   if (result != 0)
      return -1;

   if (msg->header.type == LDCS_MSG_FILE_DATA ||
       msg->header.type == LDCS_MSG_PRELOAD_FILE)
   {
      /* Optimization.  Don't read file data into heap, as it could be
         very large.  For these packets we'll postpone the network read
         until we have the file's mmap ready, then read it straight
         into that memory. */
      msg->data = NULL;
      return 0;
   } 

   if (msg->header.len) {
      buffer = (char *) malloc(msg->header.len);
      if (buffer == NULL) {
         err_printf("Error allocating space for message from network of size %lu\n", (long) msg->header.len);
         return -1;
      }
      result = spawn_net_read(channel, buffer, msg->header.len);
      if (result != 0) {
         free(buffer);
         buffer = NULL;
         return -1;
      }
   }

   msg->data = buffer;
   return 0;
}

static int setup_fe_socket(int port)
{
   int result, fd;
   struct sockaddr_in sin;
   
   fd = socket(AF_INET, SOCK_STREAM, 0);
   if (fd == -1) {
      err_printf("Error creating socket\n");
      return -1;
   }

   memset(&sin, 0, sizeof(sin));
   sin.sin_family = AF_INET;
   sin.sin_addr.s_addr = htonl(INADDR_ANY);
   sin.sin_port = htons(port);
   
   result = bind(fd, (struct sockaddr *) &sin, sizeof(sin));
   if (result == -1) {
      err_printf("Error binding socket\n");
      return -1;
   }

   result = listen(fd, 1);
   if (result == -1) {
      err_printf("Error listening socket\n");
      return -1;
   }
   
   struct sockaddr parent_addr;
   memset(&parent_addr, 0, sizeof(parent_addr));
   socklen_t parent_len = sizeof(parent_addr);
   sockfd = accept(fd, (struct sockaddr *) &parent_addr, &parent_len);
   if (sockfd == -1) {
      err_printf("Error accepting socket\n: %s", strerror(errno));
      return -1;
   }

   close(fd);
   
   int ack = 17;
   write(sockfd, &ack, sizeof(ack));

   return 0;
}

static void *select_thread(void *arg)
{
   int result = 0;
   char c = 'x';
   ssize_t write_result;
   for (;;) {
      result = waitfor_spawnnet();
      if (result == -1)
         return;

      write_result = write(write_pipe, &c, sizeof(c));
      if (write_result == -1) {
         err_printf("Failed to write to write pipe: %s", strerror(errno));
         return NULL;
      }
   }
   return NULL;
}

static void clear_pipe()
{
   char c;
   ssize_t result = read(read_pipe, &c, sizeof(c));
   if (result == -1) {
      err_printf("Failed to read from read pipe: %s", strerror(errno));
   }
}

static spawn_net_channel *selected_peer = NULL;
static int waitfor_spawnnet()
{
   //TODO: Block on spawnnet's 'select' equivalent.  Return 0
   // when data available or any change.  Return -1 when done.
   // This is called on a thread.
   //Set selected_peer to the channel with available data.
   int idx;
   int rc = spawn_net_wait(0, NULL, num_children, children, &idx);
   if (rc != 0) {
      return -1;
   }
   selected_peer = children[idx];
   return 0;
}

static int on_fe_data(int fd, int id, void *data)
{
   ldcs_process_data_t *ldcs_process_data = ( ldcs_process_data_t *) data;
   ldcs_message_t msg;
   double starttime;

   starttime = ldcs_get_time();

   int result = read(sockfd, &msg, sizeof(msg));
   if (result == -1) {
      err_printf("Unable to read from fe socket\n");
      return -1;
   }

   if (msg.header.len) {
      msg.data = malloc(msg.header.len);
      result = read(sockfd, msg.data, msg.header.len);
      if (result == -1) {
         err_printf("Unable to read from fe socket\n");
         return -1;
      }
   }
   else {
      msg.data = NULL;
   }

   result = handle_server_message(ldcs_process_data, (node_peer_t) NULL, &msg);

   ldcs_process_data->server_stat.md_cb.cnt++;
   ldcs_process_data->server_stat.md_cb.time+=(ldcs_get_time() - starttime);

   if (result == -1) {
      err_printf("Unable to read from fe socket\n");
      return -1;      
   }

   if (msg.data)
      free(msg.data);

   return 0;
}

static int on_data(int fd, int id, void *data)
{
   ldcs_process_data_t *ldcs_process_data = ( ldcs_process_data_t *) data;
   double starttime;
   int result;
   ldcs_message_t msg;
   node_peer_t peer;

   clear_pipe();
   starttime = ldcs_get_time();

   assert(selected_peer != NULL);
   peer = (node_peer_t) selected_peer;
   result = read_msg(selected_peer, &msg);
   selected_peer = NULL;
   if (result == -1)
      return -1;

   result = handle_server_message(ldcs_process_data, peer, &msg);

   ldcs_process_data->server_stat.md_cb.cnt++;
   ldcs_process_data->server_stat.md_cb.time+=(ldcs_get_time() - starttime);

   if (msg.data)
      free(msg.data);
   
   return 0;
}

int initialize_handshake_security(void *protocol)
{
#warning implement security
   return 0;
}

/* ring exchange to get left and right neighbors */
/* buffers for a PMI key/value pair, allocated during init and
 * freed in finalize */
static int   pmi_kvs_len = 0;    /* max length of PMI kvs name */
static int   pmi_key_len = 0;    /* max length of PMI key */
static int   pmi_val_len = 0;    /* max length of PMI value */
static char* pmi_kvs     = NULL; /* pointer to buffer for PMI KVS name */
static char* pmi_key     = NULL; /* pointer to buffer for PMI key */
static char* pmi_val     = NULL; /* pointer to buffer for PMI value */

/* allocate key/value buffers to use with PMI calls */
static int pmi_alloc()
{
   /* get kvs length */
   int error = PMI_Get_name_length_max(&pmi_kvs_len);
   if (error != PMI_SUCCESS) {
      err_printf("Failed to get PMI KVS length.\n");
      return -1;
   }

   /* TODO: check that this is safe */
   /* add one for trailing NUL */
   pmi_kvs_len++;

   /* Allocate space for pmi key */
   pmi_kvs = malloc(pmi_kvs_len);
   if (pmi_kvs == NULL) {
      err_printf("Failed to allocate PMI KVS buffer.\n");
      return -1;
   }

   /* Get maximum length of PMI key */
   error = PMI_KVS_Get_key_length_max(&pmi_key_len);
   if (error != PMI_SUCCESS) {
      err_printf("Failed to get PMI key length.\n");
      return -1;
   }

   /* TODO: check that this is safe */
   /* add one for trailing NUL */
   pmi_key_len++;

   /* Allocate space for pmi key */
   pmi_key = malloc(pmi_key_len);
   if (pmi_key == NULL) {
      err_printf("Failed to allocate PMI key buffer.\n");
      return -1;
   }

   /* Get maximum length of PMI value */
   error = PMI_KVS_Get_value_length_max(&pmi_val_len);
   if (error != PMI_SUCCESS) {
      err_printf("Failed to get PMI value length.\n");
      return -1;
   }

   /* TODO: check that this is safe */
   /* add one for trailing NUL */
   pmi_val_len++;

   /* Allocate space for pmi value */
   pmi_val = malloc(pmi_val_len);
   if (pmi_val == NULL) {
      err_printf("Failed to allocate PMI value buffer.\n");
      return -1;
   }

   /* TODO: need to free memory in some cases? */
   return 0;
}

/* free memory allocated for PMI key/value buffers */
static void pmi_free()
{
   /* free kvs buffer */
   if (pmi_kvs != NULL) {
      MPIU_Free(pmi_kvs);
      pmi_kvs     = NULL;
      pmi_kvs_len = 0;
   }

   /* free key buffer */
   if (pmi_key != NULL) {
      MPIU_Free(pmi_key);
      pmi_key     = NULL;
      pmi_key_len = 0;
   }

   /* free value buffer */
   if (pmi_val != NULL) {
      MPIU_Free(pmi_val);
      pmi_val     = NULL;
      pmi_val_len = 0;
   }
}

// this can be used with avalaunch but not SLURM
static int pmix_ring(spawn_net_endpoint* ep, char* name, char* left, char* right)
{
   /* insert our endpoint id into PMI */
   MPIU_Snprintf(pmi_val, pmi_val_len, "%s", name);

   /* execute the ring exchange */
   int ring_rank, ring_ranks;
   if (PMIX_Ring(pmi_val, &ring_rank, &ring_ranks, left, right) != PMI_SUCCESS) {
      err_printf("PMIX_Ring failed.\n");
      return -1;
   }

   return 0;
}

static int pmi_ring(spawn_net_endpoint* ep, char* name, char* left, char* right)
{
   int rc;

   /* insert our endpoint id into PMI */
   snprintf(pmi_key, pmi_key_len, "%d", rank);
   snprintf(pmi_val, pmi_val_len, "%s", name);
   if (PMI_KVS_Put(pmi_kvs, pmi_key, pmi_val) != PMI_SUCCESS) {
      err_printf("Failed to put PMI value.\n");
      return -1;
   }
   if (PMI_KVS_Commit(pmi_kvs) != PMI_SUCCESS) {
      err_printf("PMI commit failed.\n");
      return -1;
   }

   /* wait until all procs commit their id */
   rc = PMI_Barrier();
   if (rc != PMI_SUCCESS) {
      err_printf("PMI barrier failed.\n");
      return -1;
   }

   /* get rank of left process */
   int left_rank = rank - 1;
   if (left_rank < 0) {
      left_rank = ranks - 1;
   }

   /* get rank of right process */
   int right_rank = rank + 1;
   if (right_rank >= ranks) {
      right_rank = 0;
   }

   /* lookup address of left rank */
   snprintf(pmi_key, pmi_key_len, "%d", left_rank);
   if (PMI_KVS_Get(pmi_kvs, pmi_key, left, pmi_val_len) != PMI_SUCCESS) {
      err_printf("Failed to get left value from PMI.\n");
      return -1;
   }

   /* lookup address of right rank */
   snprintf(pmi_key, pmi_key_len, "%d", right_rank);
   if (PMI_KVS_Get(pmi_kvs, pmi_key, right, pmi_val_len) != PMI_SUCCESS) {
      err_printf("Failed to get right value from PMI.\n");
      return -1;
   }

   return 0;
}

int ldcs_audit_server_md_init(unsigned int port, unsigned int num_ports, unique_id_t unique_id, ldcs_process_data_t *data)
{
   //TODO: Initialize.  Set num_children, children array, parent.
   int rc;

   // initialize PMI
   int spawned;
   if (PMI_Init(&spawned) != PMI_SUCCESS) {
      err_printf("Failed to initialize PMI.\n");
      return -1;
   }

   // get our rank and number of ranks
//   int jobid;
   if (PMI_Get_rank(&rank) != PMI_SUCCESS) {
      err_printf("Failed to get rank from PMI.\n");
      return -1;
   }
   if (PMI_Get_size(&ranks) != PMI_SUCCESS) {
      err_printf("Failed to get size from PMI.\n");
      return -1;
   }
// if (PMI_Get_appnum(&jobid) != PMI_SUCCESS) {
//    err_printf("Failed to get jobid from PMI.\n");
//    return -1;
// }

   // open endpoint
   spawn_net_endpoint* ep = spawn_net_open(SPAWN_NET_TYPE_TCP);
   if (ep == SPAWN_NET_ENDPOINT_NULL) {
      err_printf("Failed to create spawn net endpoint.\n");
      return -1;
   }

   // get endpoint name
   char* name = spawn_net_endpoint_name(ep);
   if (name == NULL) {
      err_printf("Failed to get endpoint name.\n");
      return -1;
   }

   // allocate buffers to put and get PMI values
   rc = pmi_alloc();
   if (rc != 0) {
      err_printf("Failed to allocate PMI buffers.\n");
      return -1;
   }

   // get names of left and right endpoints
   char* left  = malloc(pmi_val_len);
   char* right = malloc(pmi_val_len);
   rc = pmi_ring(ep, name, left, right);
   if (rc != 0) {
      err_printf("Failed to exchange endpoint names with PMI.\n");
      return -1;
   }

   // create group
   group = lwgrp_create(ranks, rank, name, left, right, ep);
   if (group == NULL) {
      err_printf("Failed to create light-weight group.\n");
      return -1;
   }

   // fee left and right names
   free(left);
   free(right);

   // free pmi buffers
   pmi_free();

   // we can close our endpoint now
   if (spawn_net_close(&ep) != 0) {
      err_printf("Failed to close endpoint.\n");
      return -1;
   }

   // TODO: normally, the lwgrp structure should be opaque
   // but here we extract a binary tree from its values

   // determine number of steps in binary tree = ceil(log_2(ranks))
   int steps = 0; // number of exchanges needed to cover all ranks
   int span  = 1; // number of processes we're spanning at current step
   int count = 1; // number of ranks we're covering
   while (ranks > count) {
      steps++;
      span <<= 1;
      count += span;
   }

   // determine who our children are, we'll recursively divide full
   // range into parts depending on our rank
   num_children = 0;
   int start = 0;     // start represents rank of root of current subtree
   int end   = ranks; // end is one more than max rank in subtree
   while (steps > 0) {
      // if we're the root of the subtree, fill in our children
      if (start == rank) {
         // take next highest rank as first child
         if ((rank + 1) < end) {
            children[num_children] = group->list_right[0];
            num_children++;
         }
         // as second child, take rank that is span hops away,
         // we're careful not to double count first child if span is 1
         if (span > 1 && (rank + span) < end) {
            children[num_children] = group->list_right[steps];
            num_children++;
         }
         break;
      }

      // divide current range in two at span point
      int newstart, newend;
      if (rank < start + span) {
         // we're in the first half, update new root and end
         newstart = start + 1;
         newend   = start + span;
         if (newstart == rank) {
            // we are root in next step,
            // so we know our parent now
            parent = group->list_left[0];
         }
      } else {
         // we're in the second half, update new root
         // use same end
         newstart = start + span;
         newend   = end;
         if (newstart == rank) {
            // we are root in next step,
            // so we know our parent now
            parent = group->list_left[steps];
         }
      }
      start = newstart;
      end   = newend;

      // move on to next iteration
      span >>= 1;
      steps--;
   }

   setup_fe_socket(port);

   return -1;
}

int ldcs_audit_server_md_register_fd ( ldcs_process_data_t *data )
{
   int result;
   int pipefd[2];

   result = pipe(pipefd);
   if (result == -1) {
      err_printf("Failed to create pipe: %s\n", strerror(errno));
      return -1;
   }
   read_pipe = pipefd[0];
   write_pipe = pipefd[1];
   
   result = pthread_create(&thrd, NULL, select_thread, NULL);
   if (result == -1) {
      err_printf("Failed to spawn thread: %s\n", strerror(errno));
      return -1;
   }

   ldcs_listen_register_fd(read_pipe, read_pipe, on_data, data);

   ldcs_listen_register_fd(sockfd, sockfd, on_fe_data, data);
   return 0;
}

int ldcs_audit_server_md_unregister_fd ( ldcs_process_data_t *data )
{
   ldcs_listen_unregister_fd(read_pipe);
   ldcs_listen_unregister_fd(sockfd);
   close(read_pipe);
   close(write_pipe);
   close(sockfd);
   return 0;
}

int ldcs_audit_server_md_destroy ( ldcs_process_data_t *data )
{
   // TODO: close socket to FE?

   // free children
   if (children != NULL) {
      free(children);
   }

   // close PMI
   PMI_Finalize();

   // free group
   lwgrp_free(&group);

   return 0;
}

int ldcs_audit_server_md_is_responsible ( ldcs_process_data_t *data, char *filename )
{
#if 0
// we shut down all of these connections in lwgrp_free
// in ldcs_audit_server_md_desstroy

   int result, i;
   result = spawn_net_disconnect(&parent);
   if (result != 0) {
      err_printf("Error during spawn_net_disconnect\n");
      return -1;
   }
   for (i = 0; i < num_children; i++) {
      result = spawn_net_disconnect(children+i);
      if (result != 0) {
         err_printf("Error during spawn_net_disconnect\n");
         return -1;
      }
   }
#endif
      
   return 0;
}

int ldcs_audit_server_md_trash_bytes(node_peer_t peer, size_t size)
{
   size_t buffer_size;;
   char *buffer = NULL;
   spawn_net_channel *channel;
   int result;
   size_t cur_size;

#warning Can we throw away bytes on a smaller scale than size?
   buffer_size = size;

   buffer = malloc(buffer_size);
   while (size != 0) {
      cur_size = (size < buffer_size) ? size : buffer_size;
      result = spawn_net_read(channel, buffer, cur_size);
      if (result != 0) {
         err_printf("Error reading from spawn_net\n");
         free(buffer);
         return -1;
      }
      size -= cur_size;
   }
   free(buffer);
   return -1;
}

int ldcs_audit_server_md_forward_query(ldcs_process_data_t *ldcs_process_data, ldcs_message_t* msg)
{
   int result;
   if (rank == 0) {
      return 0;
   }
   
   result = write_msg(parent, msg);
   if (result == -1) {
      err_printf("Error writing message to parent through spawn_net\n");
      return -1;
   }

   return 0;
}

int ldcs_audit_server_md_complete_msg_read(node_peer_t peer, ldcs_message_t *msg, void *mem, size_t size)
{
   int result = 0;
   spawn_net_channel *channel = (spawn_net_channel *) peer;
   assert(msg->header.len >= size);
   
   result = spawn_net_read(channel, mem, size);
   if (result != 0) {
      err_printf("Error completing message read in spawn_net\n");
      return -1;
   }

   return 0;
}

int ldcs_audit_server_md_recv_from_parent(ldcs_message_t *msg)
{
   return read_msg(parent, msg);
}

int ldcs_audit_server_md_send(ldcs_process_data_t *ldcs_process_data, ldcs_message_t *msg, node_peer_t peer)
{
   spawn_net_channel *channel = (spawn_net_channel *) peer;
   return write_msg(channel, msg);
}

int ldcs_audit_server_md_broadcast(ldcs_process_data_t *ldcs_process_data, ldcs_message_t *msg)
{
   int i, result;
   for (i = 0; i < num_children; i++) {
      result = write_msg(children[i], msg);
      if (result == -1)
         return -1;
   }

   return 0;
}

int ldcs_audit_server_md_send_noncontig(ldcs_process_data_t *ldcs_process_data,
                                        ldcs_message_t *msg, 
                                        node_peer_t peer,
                                        void *secondary_data, size_t secondary_size)
{
   spawn_net_channel *channel = (spawn_net_channel *) peer;
   int result;

   if (!secondary_size)
      ldcs_audit_server_md_send(ldcs_process_data, msg, peer);

   assert(msg->header.len >= secondary_size);
   size_t initial_size = msg->header.len - secondary_size;
   
   //Send header
   result = spawn_net_write(channel, msg, sizeof(*msg));
   if (result != 0) {
      err_printf("Error sending noncontig header\n");
      return -1;
   }

   //Send body
   if (initial_size) {
      result = spawn_net_write(channel, msg->data, initial_size);
      if (result != 0) {
         err_printf("Error sending noncontig msg data\n");
         return -1;
      }
   }
   
   //Send secondary
   if (secondary_size) {
      result = spawn_net_write(channel, secondary_data, secondary_size);
      if (result != 0) {
         err_printf("Error sending noncontig secondary\n");
         return -1;
      }
   }

   return 0;
}

int ldcs_audit_server_md_broadcast_noncontig(ldcs_process_data_t *ldcs_process_data, ldcs_message_t *msg,
                                             void *secondary_data, size_t secondary_size)
{
   int i;
   int result;
   for (i = 0; i < num_children; i++) {
      node_peer_t peer = (node_peer_t) children[i];
      result = ldcs_audit_server_md_send_noncontig(ldcs_process_data, msg, peer, secondary_data, secondary_size);
   }
   return 0;
}

int ldcs_audit_server_md_get_num_children(ldcs_process_data_t *procdata)
{
   return num_children;
}
