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

#include "fe_comm.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netdb.h>

int initialize_handshake_security(void *protocol)
{
#warning TODO: Implement security
   return 0;
}

static int sock;

static int get_addr(char *hostname, struct in_addr *addr)
{
    struct hostent* he = gethostbyname(hostname);
    if (he) {
       *addr = *((struct in_addr *) (*he->h_addr_list));
       return 0;
    }
    addr->s_addr = inet_addr(hostname);
    if (addr->s_addr == -1) {
       err_printf("Hostname lookup failed (gethostbyname(%s))\n",
                  hostname);
       return -1;
    }
    return 0;
}

int ldcs_audit_server_fe_md_open(char **hostlist, int numhosts, unsigned int port, 
                                 unsigned int num_ports,
                                 unique_id_t unique_id, void **data)
{
   int result;
   char *first_host = hostlist[0];
   int ack;
   
   struct sockaddr_in sockaddr;
   sockaddr.sin_family = AF_INET;
   sockaddr.sin_port = htons(port);
   result = get_addr(hostlist[0], &sockaddr.sin_addr);
   if (result == -1)
      return -1;

   sock = socket(AF_INET, SOCK_STREAM, 0);
   if (sock == -1) {
      err_printf("Error creating socket\n");
      return -1;
   }
   
   int tries = 300; //30 seconds
   for (;;) {
      result = connect(sock, (struct sockaddr *) &sockaddr, sizeof(sockaddr));
      if (result == 0)
         break;
      if (tries-- == 0) {
         err_printf("Timeout connecting to lead server on %s\n", hostlist[0]);
         return -1;
      }
      usleep(100000);
   }

   result = read(sock, &ack, sizeof(&ack));

   return 0;
}

int ldcs_audit_server_fe_md_close(void *data)
{
   int result;
   ldcs_message_t msg;
   msg.header.type = LDCS_MSG_EXIT;
   msg.header.len = 0;
   msg.data = NULL;
   
   result = write(sock, &msg, sizeof(msg));
   if (result == -1) {
      err_printf("Error writing exit message\n");
      return -1;
   }

   return cobo_server_close();
}

int ldcs_audit_server_fe_broadcast(ldcs_message_t *msg, void *data)
{
   int result = write(sock, msg, sizeof(*msg));
   if (result == -1) {
      err_printf("Error broadcasting message from FE\n");
      return -1;
   }

   if (msg->header.len) {
      result = write(sock, msg->data, msg->header.len);
      if (result == -1) {
         err_printf("Error broadcasting message body from FE\n");
         return -1;
      }
   }

   return 0;
}
