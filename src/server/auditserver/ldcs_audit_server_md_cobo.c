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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/inotify.h>
#include <errno.h>
#include <assert.h>
#include <libgen.h>
#include <execinfo.h>

#include "ldcs_api.h"
#include "ldcs_api_listen.h"
#include "ldcs_audit_server_process.h"
#include "ldcs_audit_server_filemngt.h"
#include "ldcs_audit_server_md.h"
#include "ldcs_audit_server_handlers.h"
#include "ldcs_cache.h"
#include "ldcs_cobo.h"
#include "cobo_comm.h"
#include "config.h"


int ldcs_audit_server_md_cobo_CB ( int fd, int nc, void *data );
int ldcs_audit_server_md_cobo_send_msg ( int fd, ldcs_message_t *msg );

extern unique_id_t unique_id;

int cobo_rank = -1;
int cobo_size = -1;
int spindle_root_count = 1;
int spindle_root_hop   = -1;

extern int ll_read(int fd, void *buf, size_t count);

static void ldcs_audit_server_md_backtrace_print() 
{
  int j, nptrs;
  void *buffer[100];
  char **strings;

  nptrs = backtrace(buffer, 100);
  /* backtrace_symbols_fd(buffer, nptrs, STDOUT_FILENO)*/
  strings = backtrace_symbols(buffer, nptrs);
  if (strings == NULL) {
    perror("backtrace_symbols");
    exit(EXIT_FAILURE);
  }   
  for (j = 0; j < nptrs; j++) {
    debug_printf("    %s", strings[j]);
  }
  free(strings);

  return;
}

static unsigned int ldcs_audit_server_md_hashval(char* str)
{
  unsigned int hash = 5381;
  unsigned int c;
  while ((c = *str++)) {
    hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
  }
  return hash;
}

//static int ldcs_audit_server_md_get_responsible_tree_id(ldcs_process_data_t *ldcs_process_data) 
static int ldcs_audit_server_md_get_responsible_tree_id(char *path)
{
  int responsible_tree_id = COBO_PRIMARY_TREE;
  int ret;
  struct stat st;
  char *dir, *path_dup;

  /*kento*/
  /*TODO: Can I use message header for this if-else to find out
    if tree_id is for management communication or library broadcast ? */
  if (path != NULL && strlen(path) > 0) {
    path_dup = strdup(path);
    ret = stat(path_dup, &st);
    if ((st.st_mode & S_IFMT) == S_IFDIR) {
      /* path is directory */
      dir = path_dup;
    } else {
      /* path not is directory */
      dir = dirname(path_dup);
    }
    responsible_tree_id = ldcs_audit_server_md_hashval(dir) % spindle_root_count;
    free(path_dup);
  } 
  return responsible_tree_id;
}

int read_msg(int fd, node_peer_t *peer, ldcs_message_t *msg)
{
   int result;
   char *buffer = NULL;

   *peer = (node_peer_t) (long) fd;
   
   result = ll_read(fd, msg, sizeof(*msg));
   if (result == -1)
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
      result = ll_read(fd, buffer, msg->header.len);
      if (result == -1) {
         free(buffer);
         return -1;
      }
   }

   msg->data = buffer;
   return 0;
}

int ldcs_audit_server_md_init(unsigned int port, unsigned int num_ports, 
                              unique_id_t unique_id, ldcs_process_data_t *data)
{
   int rc=0;
   unsigned int *portlist;
   int my_rank, ranks, fanout;
   int i;
   char* env;

   portlist = malloc(sizeof(unsigned int) * (num_ports + 1));
   for (i = 0; i < num_ports; i++) {
      portlist[i] = port + i;
   }
   portlist[num_ports] = 0;

   /* initialize the client (read environment variables) */
   debug_printf2("Opening cobo with port %d - %d\n", portlist[0], portlist[num_ports-1]);
   if (cobo_open(unique_id, (int *) portlist, num_ports, &my_rank, &ranks) != COBO_SUCCESS) {
      printf("Failed to init\n");
      exit(1);
   }
   debug_printf2("cobo_open complete. Cobo rank %d/%d\n", my_rank, ranks);
   free(portlist);

   cobo_rank = data->server_stat.md_rank = data->md_rank = my_rank;
   cobo_size = data->server_stat.md_size = data->md_size = ranks;
   spindle_root_hop = cobo_size;
   data->md_listen_to_parent = 0;

   //   cobo_get_num_childs(&fanout);
   cobo_get_num_forest_childs(COBO_PRIMARY_TREE, &fanout);
   data->server_stat.md_fan_out = data->md_fan_out = fanout;

   cobo_barrier();
   /* send ack about being ready */
   if (data->md_rank == 0) { 
      int root_fd, ack=13;
      /* send fe client signal to stop (ack)  */
      //      cobo_get_parent_socket(&root_fd);
      cobo_get_forest_parent_socket(COBO_PRIMARY_TREE, &root_fd);
    
      ldcs_cobo_write_fd(root_fd, &ack, sizeof(ack));
      debug_printf3("sent FE client signal that server are ready %d\n",ack);
   }

   return(rc);
}

int ldcs_audit_server_md_init_post_process(unsigned int md_roots)
{
  if (md_roots > 1) {
    spindle_root_count = md_roots;
    if (spindle_root_count > cobo_size || spindle_root_count <= 0) {
      err_printf("spindle_root_count(%d) error", spindle_root_count);
      exit(1);
    }
    spindle_root_hop = cobo_size / spindle_root_count;
    if (cobo_rank == 0) {
      cobo_dbg_printf("root_count: %d root_hop: %d", spindle_root_count, spindle_root_hop);
    }
    cobo_open_forest();
  } else {
    spindle_root_count = 1;
    spindle_root_hop = cobo_size / spindle_root_count;
  }
  return 1;
}

void ldcs_audit_server_md_barrier()
{
  cobo_barrier();
  return;
}


int ldcs_audit_server_md_register_fd ( ldcs_process_data_t *ldcs_process_data ) 
{
   int rc=0, i;
   int parent_fd, child_fd;
   int num_parents, num_childs;
   
   /* Registering parents */
   cobo_dbg_printf("before 1");
   cobo_get_num_forest_parents(COBO_FOREST, &num_parents);
   cobo_dbg_printf("after 1");
   for (i = 0; i < num_parents; i++) {
     if(cobo_get_forest_parent_socket_at(i, &parent_fd)!=COBO_SUCCESS) {
       err_printf("Error, could not get parent socket\n");
       assert(0);
     }
     debug_printf3("Registering fd %d for cobo parent connection\n",parent_fd);
     ldcs_listen_register_fd(parent_fd, 0, &ldcs_audit_server_md_cobo_CB, (void *) ldcs_process_data);
   }

   /* Registering spindle_fe_main parent */
   if (ldcs_process_data->md_rank == 0) {
     if(cobo_get_forest_parent_socket(COBO_PRIMARY_TREE, &parent_fd)!=COBO_SUCCESS) {
       err_printf("Error, could not get parent socket\n");
       assert(0);
     }
     debug_printf3("Registering fd %d for cobo parent connection\n",parent_fd);
     ldcs_listen_register_fd(parent_fd, 0, &ldcs_audit_server_md_cobo_CB, (void *) ldcs_process_data);
   }
   ldcs_process_data->md_listen_to_parent=1;
   
   /* Registering childs */
   cobo_get_num_forest_childs(COBO_FOREST, &num_childs);
   for (i = 0; i<num_childs; i++) {
     cobo_get_forest_child_socket(COBO_FOREST, i , &child_fd);
     ldcs_listen_register_fd(child_fd, 0, &ldcs_audit_server_md_cobo_CB, (void *) ldcs_process_data);
   }
   return(rc);
}


int ldcs_audit_server_md_unregister_fd ( ldcs_process_data_t *ldcs_process_data ) {
   int rc=0, i;
   int parent_fd, child_fd;
   int num_parents, num_childs;
   
   /*Check if registered fds*/
   if (!ldcs_process_data->md_listen_to_parent) return rc;

   /* Registering parents */
   cobo_dbg_printf("before 2");
   cobo_get_num_forest_parents(COBO_FOREST, &num_parents);
   cobo_dbg_printf("before 3");
   for (i = 0; i < num_parents; i++) {
     if(cobo_get_forest_parent_socket_at(i, &parent_fd)!=COBO_SUCCESS) {
       err_printf("Error, could not get parent socket\n");
       assert(0);
     }
     debug_printf3("Registering fd %d for cobo parent connection\n",parent_fd);
     ldcs_listen_unregister_fd(parent_fd);
   }

   /* Registering spindle_fe_main parent */
   if (ldcs_process_data->md_rank == 0) {
     if(cobo_get_forest_parent_socket(COBO_PRIMARY_TREE, &parent_fd)!=COBO_SUCCESS) {
       err_printf("Error, could not get spindle_fe parent socket\n");
       assert(0);
     }
     debug_printf3("Registering fd %d for cobo spindle_fe parent connection\n",parent_fd);
     ldcs_listen_unregister_fd(parent_fd);
   }
   ldcs_process_data->md_listen_to_parent=0;
   
   /* Registering childs */
   cobo_get_num_forest_childs(COBO_FOREST, &num_childs);
   for (i = 0; i<num_childs; i++) {
     cobo_get_forest_child_socket(COBO_FOREST, i , &child_fd);
     ldcs_listen_unregister_fd(child_fd);
   }
   return(rc);
}


int ldcs_audit_server_md_destroy ( ldcs_process_data_t *ldcs_process_data ) 
{
   /* Nothing to be done.  Sockets will be closed when we exit. */
  /*TODO: close cobo for time measurement, but remove it*/
  if (cobo_close() != COBO_SUCCESS) {
    debug_printf3("Failed to close\n");
    exit(1);
  }
   return 0;
}

int ldcs_audit_server_md_is_responsible ( ldcs_process_data_t *ldcs_process_data, char *filename )
{
  int responsible_tree_id = ldcs_audit_server_md_get_responsible_tree_id(filename);

  if(ldcs_process_data->md_rank == responsible_tree_id) { 
    //       cobo_dbg_printf("I am responsible for file: %s (tree_id: %d)", filename, responsible_tree_id);
       debug_printf3("Decided I am responsible for file %s\n", filename);
    return 1;
  }
  return 0;
}

int ldcs_audit_server_md_forward_query(ldcs_process_data_t *ldcs_process_data, ldcs_message_t* msg) {
   int parent_fd;
   int result;
   int responsible_tree_id;
   
   responsible_tree_id = ldcs_audit_server_md_get_responsible_tree_id(ldcs_process_data->md_path);

   if (ldcs_process_data->md_rank == responsible_tree_id) {
      /* We're root--no one to forward a query to*/
      return 0;
   }
  
   cobo_get_forest_parent_socket(responsible_tree_id, &parent_fd);
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

int ldcs_audit_server_md_recv_from_parent(ldcs_message_t *msg)
{
   int fd;
   node_peer_t peer;
   cobo_get_forest_parent_socket(COBO_PRIMARY_TREE, &fd);
   return read_msg(fd, &peer, msg);
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
   int responsible_tree_id = 0;

   responsible_tree_id = ldcs_audit_server_md_get_responsible_tree_id(ldcs_process_data->md_path);

   cobo_get_num_forest_childs(responsible_tree_id, &num_childs);
   for (i = 0; i<num_childs; i++) {
     cobo_get_forest_child_socket(responsible_tree_id, i, &fd);
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
   int responsible_tree_id = 0;

   if (!secondary_size) {
     return ldcs_audit_server_md_broadcast(ldcs_process_data, msg);
   }

   responsible_tree_id = ldcs_audit_server_md_get_responsible_tree_id(ldcs_process_data->md_path);

   cobo_get_num_forest_childs(responsible_tree_id, &num_childs);
   for (i = 0; i<num_childs; i++) {
     cobo_get_forest_child_socket(responsible_tree_id, i, &fd);
     peer = (void *) (long) fd;
     result = ldcs_audit_server_md_send_noncontig(ldcs_process_data, msg, peer, secondary_data, secondary_size);
     if (result == -1)
       global_result = -1;
   }
   
   return global_result;   
}

int ldcs_audit_server_md_get_num_children(ldcs_process_data_t *procdata)
{
   int num_childs = 0;
   cobo_get_num_forest_childs(COBO_PRIMARY_TREE, &num_childs);
   return num_childs;
}
