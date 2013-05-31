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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netdb.h>
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

#include "client_heap.h"
#include "ldcs_api_socket.h"
#include "ldcs_api.h"

#define MAX_FD 1
static struct fdlist_entry_t ldcs_socket_fdlist[MAX_FD];

static int get_new_fd_socket()
{
   /* Each client should establish one connection */
   return 0;
}

static int _ldcs_read_socket(int fd, void *data, int bytes, ldcs_read_block_t block) {

  int         left,bsumread;
  size_t      btoread, bread;
  char       *dataptr;
  
  left      = bytes;
  bsumread  = 0;
  dataptr   = (char*) data;
  bread     = 0;

  while (left > 0)  {
    btoread    = left;
    bread      = read(fd, dataptr, btoread);
    if(bread<0) {
      if( (errno==EAGAIN) || (errno==EWOULDBLOCK) ) {
	debug_printf3("read from socket: got EAGAIN or EWOULDBLOCK\n");
	if(block==LDCS_READ_NO_BLOCK) return(0);
	else continue;
      } else { 
         debug_printf3("read from socket: %ld bytes ... errno=%d (%s)\n",bread,errno,strerror(errno));
      }
    } else {
      debug_printf3("read from socket: %ld bytes ...\n",bread);
    }

    if(bread>0) {
      left      -= bread;
      dataptr   += bread;
      bsumread  += bread;
    } else {
      if(bread==0) return(bsumread);
      else         return(bread);
    }
  }
  return (bsumread);
}

static int _ldcs_write_socket(int fd, const void *data, int bytes ) {
  int         left,bsumwrote;
  size_t      bwrite, bwrote;
  char       *dataptr;
  
  left      = bytes;
  bsumwrote = 0;
  dataptr   = (char*) data;

  while (left > 0) {
    bwrite     = left;
    bwrote     = write(fd, dataptr, bwrite);
    left      -= bwrote;
    dataptr   += bwrote;
    bsumwrote += bwrote;
  }
  return (bsumwrote);
}


int client_open_connection_socket(char* hostname, int portno)
{
   int sockfd, fd;
   struct sockaddr_in serv_addr;
   struct hostent *server;

   fd=get_new_fd_socket();
   if(fd<0) return(-1);
   ldcs_socket_fdlist[fd].type=LDCS_SOCKET_FD_TYPE_CONN;

   sockfd = socket(AF_INET, SOCK_STREAM, 0);
   if (sockfd < 0) _error("ERROR opening socket");

   debug_printf3("after socket: -> sockfd=%d\n",sockfd);

   server = gethostbyname(hostname);
   debug_printf3("after gethostbyname: -> server=%p\n",server);
   if (server == NULL) {
      fprintf(stderr,"ERROR, no such host\n");
      exit(0);
   }

   debug_printf3("after gethostbyname: -> hostname=%s\n",server->h_name);

   bzero((char *) &serv_addr, sizeof(serv_addr));
   serv_addr.sin_family = AF_INET;
   bcopy((char *)server->h_addr, 
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
   serv_addr.sin_port = htons(portno);
   debug_printf3("after port: -> port=%d\n",portno);

   ldcs_socket_fdlist[fd].fd=sockfd;


   if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) {
      return(-1);
   }
  
   debug_printf3("after connect: -> port=%d\n",portno);
  
   return(fd);
  
}

char *client_get_connection_string_socket(int fd)
{
   int sockfd = ldcs_socket_fdlist[fd].fd;

   int slen = 64;
   char *str = (char *) spindle_malloc(slen);
   if (!str)
      return NULL;
   snprintf(str, slen, "%d", sockfd);
   return str;
}

int client_register_connection_socket(char *connection_str)
{
   int sockfd, result;
   
   result = sscanf(connection_str, "%d", &sockfd);
   if (result != 1)
      return -1;

   int fd = get_new_fd_socket();
   if (fd < 0)
      return -1;

   ldcs_socket_fdlist[fd].type = LDCS_SOCKET_FD_TYPE_CONN;
   ldcs_socket_fdlist[fd].fd = sockfd;

   return fd;
}

int client_close_connection_socket(int fd) 
{
   int rc=0;
   int sockfd;

   if ((fd<0) || (fd>MAX_FD) )  _error("wrong fd");

   sockfd=ldcs_socket_fdlist[fd].fd;

   if(sockfd<0) return(-1);
   
   rc=close(sockfd);

   return(rc);
}

int client_send_msg_socket(int fd, ldcs_message_t * msg) {

  char help[41];
  int n, connfd;
  if ((fd<0) || (fd>MAX_FD) )  _error("wrong fd");
  connfd=ldcs_socket_fdlist[fd].fd;

  bzero(help,41);if(msg->data) strncpy(help,msg->data,40);
  debug_printf3("sending message of type: %s len=%d data=%s ...\n",
	       _message_type_to_str(msg->header.type),
	       msg->header.len,help );

  n = _ldcs_write_socket(connfd,&msg->header,sizeof(msg->header));
  if (n < 0) _error("ERROR writing header to socket");

  if(msg->header.len>0) {
    n = _ldcs_write_socket(connfd,(void *) msg->data,msg->header.len);
    if (n < 0) _error("ERROR writing data to socket");
  }
  
  
  return(0);
}

int client_recv_msg_static_socket(int fd, ldcs_message_t *msg,  ldcs_read_block_t block) {
  char help[41];
  int rc=0;
  int n, connfd;
  if ((fd<0) || (fd>MAX_FD) )  _error("wrong fd");
  connfd=ldcs_socket_fdlist[fd].fd;


  n = _ldcs_read_socket(connfd,&msg->header,sizeof(msg->header), block);
  if (n == 0) return(rc);
  if (n < 0) _error("ERROR reading header from socket");

  if(msg->header.len>0) {

    msg->data = (char *) spindle_malloc(msg->header.len);
    if (!msg)  _error("could not allocate memory for message data");
    
    n = _ldcs_read_socket(connfd,msg->data,msg->header.len, LDCS_READ_BLOCK);
    if (n == 0) return(rc);
    if (n < 0) _error("ERROR reading message data from socket");
    if (n != msg->header.len) _error("received different number of bytes for message data");

  } else {
    msg->data = NULL;
  }

  bzero(help,41);if(msg->data) strncpy(help,msg->data,40);
  debug_printf3("received message of type: %s len=%d data=%s ...\n",
	       _message_type_to_str(msg->header.type),
	       msg->header.len,help );
  
  return(0);
}
