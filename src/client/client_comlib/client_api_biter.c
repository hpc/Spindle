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

#define _GNU_SOURCE


#include <assert.h>
#include <stdlib.h>

#include "biterc.h"
#include "ldcs_api.h"
#include "config.h"
#include "spindle_debug.h"
#include "client_heap.h"

static int session = -1;
static char *cached_location;

//#warning Move BITERC_SHM_SIZE to config parameter
#define BITERC_SHM_SIZE 2*1024*1024
int client_open_connection_biter(char* location, int number)
{
   debug_printf3("Calling biterc_newsession(%s, %lu)\n", location, (unsigned long) BITERC_SHM_SIZE);
   session = biterc_newsession(location, BITERC_SHM_SIZE);
   if (session == -1) {
      err_printf("Client failed to create connection: %s\n", biterc_lasterror_str());
   }
   cached_location = location;
   return session;
}

int client_close_connection_biter(int connid)
{
   if (connid != session) {
      err_printf("Invalid connection ID passed to client_close_connection_biter\n");
      return -1;
   }
   session = -1;
   return 0;
}

int client_register_connection_biter(char *connection_str)
{
   assert(0 && "No fork or exec support with biter");
}

char *client_get_connection_string_biter(int fd)
{
   assert(0 && "No fork or exec support with biter");
}

int client_send_msg_biter(int connid, ldcs_message_t *msg)
{
   int result;

   if (connid != session) {
      err_printf("Invalid connection ID passed to client_send_msg_biter\n");
      return -1;
   }

   debug_printf3("sending message of size len=%d\n", msg->header.len);

   result = biterc_write(connid, &msg->header, sizeof(msg->header));
   if (result == -1) {
      err_printf("Error writing message header in biter client\n");
      return -1;
   }

   if (msg->header.len == 0)
      return 0;

   result = biterc_write(connid, msg->data, msg->header.len);
   if (result == -1) {
      err_printf("Error writing message body in biter client\n");
      return -1;
   }   

   return 0;
}

static int client_recv_msg_biter(int connid, ldcs_message_t *msg, ldcs_read_block_t block, int is_dynamic)
{
   int result;

   msg->header.type=LDCS_MSG_UNKNOWN;
   msg->header.len=0;

   if (connid != session) {
      err_printf("Invalid connection ID passed to client_recv_msg_biter\n");
      return -1;
   }

   assert(block == LDCS_READ_BLOCK); /* Non-blocking isn't implemented yet */

   debug_printf3("Reading %d bytes for header from biter\n", (int) sizeof(msg->header));
   result = biterc_read(connid, &msg->header, sizeof(msg->header));
   if (result == -1) {
      err_printf("Error reading message header in biter client: %s\n", biterc_lasterror_str());
      return -1;
   }
   if (result == 0) {
      err_printf("EOF reading message header in biter client: %s\n", biterc_lasterror_str());
      return -1;
   }
   
   if (msg->header.len == 0) {
      msg->data = NULL;
      return 0;
   }

   if (is_dynamic) {
      msg->data = (char *) spindle_malloc(msg->header.len);
      assert(msg->data);
   }

   debug_printf3("Reading %d bytes for body from biter\n", (int) msg->header.len);
   result = biterc_read(connid, msg->data, msg->header.len);
   if (result == -1)
      err_printf("Error reading message body in biter client: %s\n", biterc_lasterror_str());
   return result;
}

int client_recv_msg_static_biter(int fd, ldcs_message_t *msg, ldcs_read_block_t block)
{
   return client_recv_msg_biter(fd, msg, block, 0);
}

int client_recv_msg_dynamic_biter(int fd, ldcs_message_t *msg, ldcs_read_block_t block)
{
   return client_recv_msg_biter(fd, msg, block, 1);
}

int is_client_fd(int connid, int fd)
{
   return biterc_is_client_fd(session, fd);
}
