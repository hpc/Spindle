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

static size_t num_children;
static spawn_net_channel **children;
static spawn_net_channel *parent;

static unsigned long rank;
static unsigned long size;

static int write_msg(spawn_net_channel *channel, ldcs_message_t *msg)
{
   int result;
   result = spawn_net_write(channel, msg, sizeof(*msg));
   if (result != 0) {
      err_printf("Error writing spawnnet header\n");
      return -1;
   }

   result = spawn_net_write(channel, msg->data, msg->header.len);
   if (result != 0) {
      err_printf("Error writing spawnnet body\n");
      return -1;
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

   if (msg->header.type == LDCS_MSG_FILE_DATA || msg->header.type == LDCS_MSG_PRELOAD_FILE) {
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

int ldcs_audit_server_md_init(unsigned int port, unsigned int num_ports, unique_id_t unique_id, ldcs_process_data_t *data)
{
   //TODO: Initialize.  Set num_children, children array, parent.
   // Set rank and size.
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
   return 0;
}

int ldcs_audit_server_md_is_responsible ( ldcs_process_data_t *data, char *filename )
{
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
