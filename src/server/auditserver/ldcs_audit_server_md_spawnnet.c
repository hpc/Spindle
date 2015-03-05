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

#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <stdlib.h>

static int waitfor_spawnnet();

static int read_pipe;
static int write_pipe;
static pthread_t thrd;

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

static int waitfor_spawnnet()
{
   //TODO: Block on spawnnet's 'select' equivalent.  Return 0
   // when data available or any change.  Return -1 when done.
   // This is called on a thread.
   return 0;
}

static int on_data(int fd, int id, void *data)
{
   ldcs_process_data_t *ldcs_process_data = ( ldcs_process_data_t *) data;
   double starttime;
   int result;

   clear_pipe();
   starttime = ldcs_get_time();

   ldcs_message_t msg;
   node_peer_t peer;
   //TODO: Read a message into msg.  Peer is who sent the packet

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
      
   return 0;
}

int ldcs_audit_server_md_unregister_fd ( ldcs_process_data_t *data )
{
  return -1;
}

int ldcs_audit_server_md_destroy ( ldcs_process_data_t *data )
{
  return -1;
}

int ldcs_audit_server_md_is_responsible ( ldcs_process_data_t *data, char *filename )
{
  return -1;
}

int ldcs_audit_server_md_trash_bytes(node_peer_t peer, size_t size)
{
  return -1;
}

int ldcs_audit_server_md_forward_query(ldcs_process_data_t *ldcs_process_data, ldcs_message_t* msg)
{
  return -1;
}

int ldcs_audit_server_md_complete_msg_read(node_peer_t peer, ldcs_message_t *msg, void *mem, size_t size)
{
  return -1;
}

int ldcs_audit_server_md_recv_from_parent(ldcs_message_t *msg)
{
  return -1;
}

int ldcs_audit_server_md_send(ldcs_process_data_t *ldcs_process_data, ldcs_message_t *msg, node_peer_t peer)
{
  return -1;
}

int ldcs_audit_server_md_broadcast(ldcs_process_data_t *ldcs_process_data, ldcs_message_t *msg)
{
  return -1;
}

int ldcs_audit_server_md_send_noncontig(ldcs_process_data_t *ldcs_process_data, ldcs_message_t *msg, 
                                        node_peer_t peer,
                                        void *secondary_data, size_t secondary_size)
{
  return -1;
}

int ldcs_audit_server_md_broadcast_noncontig(ldcs_process_data_t *ldcs_process_data, ldcs_message_t *msg,
                                             void *secondary_data, size_t secondary_size)
{
  return -1;
}

int ldcs_audit_server_md_get_num_children(ldcs_process_data_t *procdata)
{
  return -1;
}
