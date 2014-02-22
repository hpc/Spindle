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

#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <assert.h>

#include "biterd.h"
#include "ldcs_api.h"
#include "config.h"
#include "spindle_debug.h"
#include "ldcs_audit_server_process.h"

static int num_cns;
static char *tmpdir;
static int number;
static int *num_live_procs;
static uint16_t **session_proc_to_nc;
static int server_id = INT32_MAX;
static int clients_avail_r_fd;
static int clients_avail_w_fd;

#define NEW_CLIENT_ID(SESSION, PROC) ((((uint16_t) SESSION) << 16) | ((uint16_t) PROC))
#define CLIENT_ID_SESSION(CLIENTID) ((int) (CLIENTID >> 16))
#define CLIENT_ID_PROC(CLIENTID) ((int) (CLIENTID & ((1 << 16)-1)))

int ldcs_create_server_biter(char* location, int num)
{
   int clients_avail[2];
   int result;
   char a_byte = 'a';

   num_cns = biterd_num_compute_nodes();
   assert(num_cns > 0);

   num_live_procs = calloc(num_cns, sizeof(*num_live_procs));
   session_proc_to_nc = calloc(num_cns, sizeof(*session_proc_to_nc));

   tmpdir = location;
   number = num;

   result = pipe(clients_avail);
   assert(result != -1);
   clients_avail_r_fd = clients_avail[0];
   clients_avail_w_fd = clients_avail[1];
   write(clients_avail_w_fd, &a_byte, sizeof(a_byte));
   
   return server_id;
}

int ldcs_open_server_connection_biter(int id)
{
   assert(0 && "Unused");
   return -1;
}

int ldcs_open_server_connections_biter(int fd, int nc, int *more_avail)
{
   static int cur_session = -1;
   static int cur_proc_in_cn = 0;
   static int num_procs_in_cn = 0;
   int result, new_client, i;
   char a_byte;
   
   assert(fd == server_id);

   if (cur_proc_in_cn == num_procs_in_cn) {
      cur_session++;
      debug_printf3("Creating new biterd_session %d\n", cur_session+1);
      result = biterd_newsession(tmpdir, cur_session);
      if (result == -1) {
         err_printf("Error creating new biterd session %d: %s", cur_session, biterd_lasterror_str());
         return -1;
      }
      assert(cur_session == result);
      cur_proc_in_cn = 0;
      num_procs_in_cn = biterd_num_clients(cur_session);
      assert(num_procs_in_cn > 0);
   }


   new_client = NEW_CLIENT_ID(cur_session, cur_proc_in_cn);
   if (session_proc_to_nc[cur_session] == NULL) {
      session_proc_to_nc[cur_session] = malloc(num_procs_in_cn * sizeof(*(session_proc_to_nc[cur_session])));
      for (i = 0; i < num_procs_in_cn; i++)
         session_proc_to_nc[cur_session][i] = -1;
   }
   session_proc_to_nc[cur_session][cur_proc_in_cn] = nc;

   num_live_procs[cur_session]++;

   cur_proc_in_cn++;
   *more_avail = (cur_session < num_cns-1) || (cur_proc_in_cn < num_procs_in_cn);
   if (!*more_avail) {
      read(clients_avail_r_fd, &a_byte, sizeof(a_byte));
   }

   return new_client;
}

int ldcs_close_server_connection_biter(int connid)
{
   int session = CLIENT_ID_SESSION(connid);
   int proc = CLIENT_ID_PROC(connid);

   assert(connid != server_id);
   assert(num_live_procs[session] > 0);

   debug_printf3("Closing server connection to session %d, client %d\n", session, proc);

   session_proc_to_nc[session][proc] = -1;
   num_live_procs[session]--;
   if (num_live_procs[session] == 0) {
      debug_printf3("Closing server connection to session %d\n", session);
      biterd_clean(session);
   }

   return 0;
}

int ldcs_destroy_server_biter(int cid)
{
   int i;
   assert(cid == server_id);
   debug_printf3("Closing connections to all servers\n");
   for (i = 0; i < num_cns; i++) {
      if (num_live_procs[i] > 0) {
         debug_printf3("Closing server connections to session %d\n", i);
         biterd_clean(i);
         num_live_procs[i] = 0;
      }
   }

   return 0;
}

int ldcs_send_msg_biter(int fd, ldcs_message_t *msg)
{
   int session = CLIENT_ID_SESSION(fd);
   int proc = CLIENT_ID_PROC(fd);
   int result;

   assert(fd != server_id);
   assert(session < num_cns);
   assert(num_live_procs[session]);
   assert(proc >= 0 && proc < biterd_num_clients(session));
   
   if (spindle_debug_prints) {
      int rank = biterd_get_rank(session, proc);
      debug_printf3("Sending message of size %d to rank %d (session = %d, proc = %d)\n",
                    msg->header.len, rank, session, proc);
   }
   
   result = biterd_write(session, proc, &msg->header, sizeof(msg->header));
   if (result == -1) {
      err_printf("Error writing header to session %d, proc %d: %s\n",
                 session, proc, biterd_lasterror_str());
      return -1;
   }
   
   if (msg->header.len == 0)
      return 0;

   result = biterd_write(session, proc, msg->data, msg->header.len);
   if (result == -1) {
      err_printf("Error writing message of size %d to session %d, proc %d: %s\n",
                 msg->header.len, session, proc, biterd_lasterror_str());
      return -1;
   }

   return 0;
}

int ldcs_get_fd_biter(int connid) 
{
   int session, proc;

   if (connid == server_id)
      return clients_avail_r_fd;

   session = CLIENT_ID_SESSION(connid);
   proc = CLIENT_ID_PROC(connid);

   if (proc != 0)
      return -1;
   
   return biterd_get_fd(session);
}

int ldcs_get_aux_fd_biter()
{
   return biterd_get_aux_fd();
}

int ldcs_socket_id_to_nc_biter(int id, int fd)
{
   int session, proc, result;

   if (id == CLIENT_CB_AUX_FD) {
      result = biterd_get_session_proc_w_aux_data(&session, &proc);
      if (result == -1) {
         err_printf("Error finding process with aux data\n");
         return -1;
      }
   }
   else {
      session = CLIENT_ID_SESSION(id);
      proc = biterd_find_client_w_data(session);
      if (proc == -1) {
         err_printf("Error finding client with data in session\n");
         return -1;
      }
      assert(biterd_get_fd(session) == fd);
   }
   assert(session >= 0 && session < num_cns);
   assert(proc >= 0 && proc < biterd_num_clients(session));
   
   result = session_proc_to_nc[session][proc];
   assert(result != -1);
   return result;
}

int ldcs_recv_msg_static_biter(int connid, ldcs_message_t *msg, ldcs_read_block_t block)
{
   int session = CLIENT_ID_SESSION(connid);
   int proc = CLIENT_ID_PROC(connid);
   int result;

   assert(connid != server_id);
   assert(session < num_cns);
   assert(num_live_procs[session]);
   assert(proc < biterd_num_clients(session));
   
   msg->header.type = LDCS_MSG_UNKNOWN;
   msg->header.len = 0;
   
   if (spindle_debug_prints) {
      int rank = biterd_get_rank(session, proc);
      debug_printf3("Receiving message from rank %d (session = %d, proc = %d)\n",
                    rank, session, proc);
   }
   
   result = biterd_read(session, proc, &msg->header, sizeof(msg->header));
   if (result == -1) {
      err_printf("Error reading header from session %d, proc %d: %s\n",
                 session, proc, biterd_lasterror_str());
      return -1;
   }

   debug_printf3("Message to be read is of size %d (session = %d, proc = %d)\n",
                 msg->header.len, session, proc);
   
   if (msg->header.len == 0) {
      msg->data = NULL;
      return 0;
   }

   result = biterd_read(session, proc, msg->data, msg->header.len);
   if (result == -1) {
      err_printf("Error reading message of size %d from session %d, proc %d: %s\n",
                 msg->header.len, session, proc, biterd_lasterror_str());
      return -1;
   }

   return 0;   
}
