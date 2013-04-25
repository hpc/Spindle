/*
  This file is part of Spindle.  For copyright information see the COPYRIGHT 
  file in the top level directory, or at 
  <TODO:URL>.

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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/inotify.h>
#include <errno.h>
#include <assert.h>

#include "ldcs_api.h"
#include "ldcs_api_listen.h"
#include "ldcs_audit_server_process.h"
#include "ldcs_audit_server_filemngt.h"
#include "ldcs_audit_server_md.h"
#include "ldcs_audit_server_handlers.h"
#include "ldcs_cache.h"
#include "ldcs_cobo.h"
#include "cobo_comm.h"

int ldcs_audit_server_md_cobo_CB ( int fd, int nc, void *data );
int ldcs_audit_server_md_cobo_send_msg ( int fd, ldcs_message_t *msg );

static int ll_read(int fd, void *buf, size_t count)
{
   int result;
   size_t pos = 0;

   while (pos < count) {
      result = read(fd, ((char *) buf) + pos, count - pos);
      debug_printf3("Read %d bytes from network: %d %d %d...\n", result, (int) ((char *)buf)[pos],
                    (int) ((char *)buf)[pos+1], (int) ((char *)buf)[pos+2]);
      if (result == -1 || result == 0) {
         if (errno == EINTR || errno == EAGAIN)
            continue;
         err_printf("Error reading from cobo FD %d\n", fd);
         return -1;
      }
      pos += result;
   }
   return 0;
}

static int ll_write(int fd, void *buf, size_t count)
{
   int result;
   size_t pos = 0;

   while (pos < count) {
      result = write(fd, ((char *) buf) + pos, count - pos);
      debug_printf3("Wrote %d bytes to network: %d %d %d...\n", result, (int) ((char *)buf)[pos],
                    (int) ((char *)buf)[pos+1], (int) ((char *)buf)[pos+2]);

      if (result == -1 || result == 0) {
         if (errno == EINTR || errno == EAGAIN)
            continue;
         err_printf("Error writing to cobo FD %d\n", fd);
         return -1;
      }
      pos += result;
   }
   return 0;
}

static int read_msg(int fd, node_peer_t *peer, ldcs_message_t *msg)
{
   int result;
   char *buffer = NULL;

   *peer = (node_peer_t) (long) fd;
   
   result = ll_read(fd, msg, sizeof(*msg));
   if (result == -1)
      return -1;

   if (msg->header.type == LDCS_MSG_FILE_DATA) {
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
      result = ll_read(fd, buffer, msg->header.len);
      if (result == -1) {
         free(buffer);
         return -1;
      }
   }

   msg->data = buffer;
   return 0;
}

static int write_msg(int fd, ldcs_message_t *msg)
{
   int result = ll_write(fd, msg, sizeof(*msg));
   if (result == -1) {
      return -1;
   }

   if (msg->header.len && msg->data) {
      result = ll_write(fd, msg->data, msg->header.len);
      if (result == -1) {
         return -1;
      }
   }

   return 0;
}

int ldcs_audit_server_md_init ( ldcs_process_data_t *ldcs_process_data ) {
   int rc=0;

   int* portlist = NULL;
   int num_ports = 20;
   int i, my_rank, ranks, fanout;

   portlist = malloc(num_ports * sizeof(int));

   for (i=0; i<num_ports; i++) {
      portlist[i] = 5000 + i;
   }

   /* initialize the client (read environment variables) */
   debug_printf2("Opening cobo\n");
   if (cobo_open(2384932, portlist, num_ports, &my_rank, &ranks) != COBO_SUCCESS) {
      printf("Failed to init\n");
      exit(1);
   }
   debug_printf2("cobo_open complete. Cobo rank %d/%d\n", my_rank, ranks);

   ldcs_process_data->server_stat.md_rank=ldcs_process_data->md_rank=my_rank;
   ldcs_process_data->server_stat.md_size=ldcs_process_data->md_size=ranks;
   ldcs_process_data->md_listen_to_parent=0;

   cobo_get_num_childs(&fanout);
   ldcs_process_data->server_stat.md_fan_out=ldcs_process_data->md_fan_out = fanout;

   free(portlist);

   cobo_barrier();

   /* send ack about being ready */
   if (ldcs_process_data->md_rank == 0) { 
      int root_fd, ack=13;

      /* send fe client signal to stop (ack)  */
      cobo_get_parent_socket(&root_fd);
    
      ldcs_cobo_write_fd(root_fd, &ack, sizeof(ack));
      debug_printf3("sent FE client signal that server are ready %d\n",ack);
   }
  
   return(rc);
}

int ldcs_audit_server_md_register_fd ( ldcs_process_data_t *ldcs_process_data ) {
   int rc=0, i;
   int parent_fd, child_fd;
   int num_childs;

   if(cobo_get_parent_socket(&parent_fd)!=COBO_SUCCESS) {
      err_printf("Error, could not get parent socket\n");
      assert(0);
   }

   debug_printf3("Registering fd %d for cobo parent connection\n",parent_fd);
   ldcs_listen_register_fd(parent_fd, 0, &ldcs_audit_server_md_cobo_CB, (void *) ldcs_process_data);
   ldcs_process_data->md_listen_to_parent=1;
   
   cobo_get_num_childs(&num_childs);
   for (i = 0; i<num_childs; i++) {
      cobo_get_child_socket(i, &child_fd);
      ldcs_listen_register_fd(child_fd, 0, &ldcs_audit_server_md_cobo_CB, (void *) ldcs_process_data);
   }
   
   return(rc);
}

int ldcs_audit_server_md_unregister_fd ( ldcs_process_data_t *ldcs_process_data ) {
   int rc=0, i;
   int parent_fd, child_fd;
   int num_childs;
   if(ldcs_process_data->md_listen_to_parent) {
      if(cobo_get_parent_socket(&parent_fd)!=COBO_SUCCESS) {
         _error("cobo internal error (parent socket)");
      }
      ldcs_process_data->md_listen_to_parent=0;
      ldcs_listen_unregister_fd(parent_fd);

      cobo_get_num_childs(&num_childs);
      for (i = 0; i<num_childs; i++) {
         cobo_get_child_socket(i, &child_fd);
         ldcs_listen_unregister_fd(child_fd);
      }
   }

   return(rc);
}

int ldcs_audit_server_md_destroy ( ldcs_process_data_t *ldcs_process_data ) 
{
   /* Nothing to be done.  Sockets will be closed when we exit. */
   return 0;
}

int ldcs_audit_server_md_is_responsible ( ldcs_process_data_t *ldcs_process_data, char *filename ) {
   /* current implementation: only MD rank does file operations  */
   if(ldcs_process_data->md_rank == 0) { 
      debug_printf3("Decided I am responsible for file %s\n", filename);
      return 1;
   } else {
      debug_printf3("Decided I am not responsible for file %s\n", filename);
      return 0;
   }
}

int ldcs_audit_server_md_forward_query(ldcs_process_data_t *ldcs_process_data, ldcs_message_t* msg) {
   int parent_fd;
   int result;
   if (ldcs_process_data->md_rank == 0) {
      /* We're root--no one to forward a query to*/
      return 0;
   }
  
   cobo_get_parent_socket(&parent_fd);
   result = write_msg(parent_fd, msg);
   if (result < 0) {
      err_printf("Problem writing message to parent, result is %d\n", result);
      return -1;
   }
  
   return 0;
}

int ldcs_audit_server_md_complete_msg_read(node_peer_t peer, ldcs_message_t *msg, void *mem, size_t size)
{
   int result = 0;
   int fd = (int) (long) peer;
   assert(msg->header.len >= size);
   result = ll_read(fd, mem, size);
   if (result == -1)
      return -1;
   return 0;
}

int ldcs_audit_server_md_trash_bytes(node_peer_t peer, size_t size)
{
   char buffer[4096];
   int fd = (int) (long) peer;

   while (size) {
      if (size < 4096) {
         ll_read(fd, buffer, size);
         size = 0;
      }
      else {
         ll_read(fd, buffer, 4096);
         size -= 4096;
      }
   }

   return 0;
}

int ldcs_audit_server_md_cobo_CB(int fd, int nc, void *data)
{
   int rc=0;
   ldcs_process_data_t *ldcs_process_data = ( ldcs_process_data_t *) data ;
   ldcs_message_t msg;
   double starttime = ldcs_get_time();
   node_peer_t peer;
  
   /* receive msg from cobo network */
   rc = read_msg(fd, &peer, &msg);
   if (rc == -1)
      return -1;

   rc = handle_server_message(ldcs_process_data, peer, &msg);

   ldcs_process_data->server_stat.md_cb.cnt++;
   ldcs_process_data->server_stat.md_cb.time+=(ldcs_get_time() - starttime);

   if (msg.data)
      free(msg.data);

   return(rc);
}

int ldcs_audit_server_fe_md_open ( char **hostlist, int numhosts, void **data  ) {
   int rc=0;
   int num_ports   = 20;
   int* portlist = malloc(num_ports * sizeof(int));
   int i;
   int root_fd, ack;

   /* build our portlist */
   for (i=0; i<num_ports; i++) {
      portlist[i] = 5000 + i;
   }

   cobo_server_open(2384932, hostlist, numhosts, portlist, num_ports);

   cobo_server_get_root_socket(&root_fd);
  
   ldcs_cobo_read_fd(root_fd, &ack, sizeof(ack));

   return(rc);
}

int ldcs_audit_server_fe_md_close ( void *data  ) {
  
   ldcs_message_t out_msg;
   int root_fd;

   debug_printf("Sending exit message to daemons\n");
   out_msg.header.type = LDCS_MSG_EXIT;
   out_msg.data = NULL;

   cobo_server_get_root_socket(&root_fd);
   write_msg(root_fd, &out_msg);
         
   return cobo_server_close();
}

int ldcs_audit_server_md_send(ldcs_process_data_t *ldcs_process_data, ldcs_message_t *msg, node_peer_t peer)
{
   int fd = (int) (long) peer;
   return write_msg(fd, msg);
}

int ldcs_audit_server_md_send_noncontig(ldcs_process_data_t *ldcs_process_data, ldcs_message_t *msg, 
                                        node_peer_t peer,
                                        void *secondary_data, size_t secondary_size)
{
   int fd; 
   if (!secondary_size)
      return ldcs_audit_server_md_send(ldcs_process_data, msg, peer);

   assert(msg->header.len >= secondary_size);
   size_t initial_size = msg->header.len - secondary_size;
   
   /* Send header */
   fd = (int) (long) peer;
   int result = ll_write(fd, msg, sizeof(*msg));
   if (result == -1) {
      return -1;
   }

   if (initial_size) {
      /* Send initial part of data */
      assert(msg->data);
      result = ll_write(fd, msg->data, initial_size);
      if (result == -1) {
         return -1;
      }
   }

   /* Send the secondary data */
   result = ll_write(fd, secondary_data, secondary_size);
   if (result == -1) {
      return -1;
   }

   return 0;
}

int ldcs_audit_server_md_broadcast(ldcs_process_data_t *ldcs_process_data, ldcs_message_t *msg)
{
   int fd, i;
   int result, global_result = 0;
   int num_childs = 0;

   cobo_get_num_childs(&num_childs);
   for (i = 0; i<num_childs; i++) {
      cobo_get_child_socket(i, &fd);
      result = write_msg(fd, msg);
      if (result == -1)
         global_result = -1;
   }
   
   return global_result;
}

int ldcs_audit_server_md_broadcast_noncontig(ldcs_process_data_t *ldcs_process_data, ldcs_message_t *msg,
                                             void *secondary_data, size_t secondary_size)
{
   int fd, i;
   int result, global_result = 0;
   int num_childs = 0;
   node_peer_t peer;

   if (!secondary_size)
      return ldcs_audit_server_md_broadcast(ldcs_process_data, msg);

   cobo_get_num_childs(&num_childs);

   for (i = 0; i<num_childs; i++) {
      cobo_get_child_socket(i, &fd);
      peer = (void *) (long) fd;
      result = ldcs_audit_server_md_send_noncontig(ldcs_process_data, msg, peer, secondary_data, secondary_size);
      if (result == -1)
         global_result = -1;
   }
   
   return global_result;   
}

