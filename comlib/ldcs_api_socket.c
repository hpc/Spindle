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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 

#include "ldcs_api.h"
#include "ldcs_api_socket.h"


/* ************************************************************** */
/* FD list                                                        */
/* ************************************************************** */

#define MAX_FD 100

struct fdlist_entry_t ldcs_socket_fdlist[MAX_FD];
int ldcs_fdlist_socket_cnt=-1;

int get_new_fd_socket () {
  int fd;
  if(ldcs_fdlist_socket_cnt==-1) {
    /* init fd list */
    for(fd=0;fd<MAX_FD;fd++) ldcs_socket_fdlist[fd].inuse=0;
  }
  if(ldcs_fdlist_socket_cnt+1<MAX_FD) {

    fd=0;
    while ( (fd<MAX_FD) && (ldcs_socket_fdlist[fd].inuse==1) ) fd++;
    ldcs_socket_fdlist[fd].inuse=1;
    ldcs_fdlist_socket_cnt++;
    return(fd);
  } else {
    return(-1);
  }
}

void free_fd_socket (int fd) {
    ldcs_socket_fdlist[fd].inuse=0;
    ldcs_fdlist_socket_cnt--;
}

/* end of fd list */


int ldcs_get_fd_socket (int fd) {
  int realfd=-1;
  if ((fd<0) || (fd>MAX_FD) )  _error("wrong fd");
  if(ldcs_socket_fdlist[fd].inuse) {
    if(ldcs_socket_fdlist[fd].type==LDCS_SOCKET_FD_TYPE_SERVER) {
      realfd=ldcs_socket_fdlist[fd].server_fd;
    }
    if(ldcs_socket_fdlist[fd].type==LDCS_SOCKET_FD_TYPE_CONN) {
      realfd=ldcs_socket_fdlist[fd].fd;
    }
  }
  return(realfd);
}

/* ************************************************************** */
/* SERVER functions                                               */
/* ************************************************************** */


int ldcs_create_server_socket(char* location, int number) {
  int fd, sockfd;
  struct sockaddr_in serv_addr;

  fd=get_new_fd_socket();
  if(fd<0) return(-1);

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)  _error("ERROR opening socket");
  
  debug_printf3("after socket: -> sockfd=%d\n",sockfd);
  
  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family      = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port        = htons(number);
  if (bind(sockfd, (struct sockaddr *) &serv_addr,
	   sizeof(serv_addr)) < 0)  {
    debug_printf3("after bind: -> could not bind %d \n",number);
    return(-1);
  }
  
  debug_printf3("after bind: -> sockfd=%d\n",sockfd);

  listen(sockfd,5);
  
  debug_printf3("after listen: -> sockfd=%d\n",sockfd);

  ldcs_socket_fdlist[fd].type=LDCS_SOCKET_FD_TYPE_SERVER;
  ldcs_socket_fdlist[fd].server_fd=sockfd;
  ldcs_socket_fdlist[fd].conn_list=NULL;
  ldcs_socket_fdlist[fd].conn_list_size=0;
  ldcs_socket_fdlist[fd].conn_list_used=0;

  return(fd);
}

int ldcs_open_server_connection_socket(int fd) {
  struct sockaddr_in cli_addr;
  socklen_t clilen;
  int connfd, serverfd, newsockfd;

  if ((fd<0) || (fd>MAX_FD) )  _error("wrong fd");
  serverfd=ldcs_socket_fdlist[fd].server_fd;
  if(serverfd<0) return(-1);

  clilen    = sizeof(cli_addr);
  newsockfd = accept(serverfd, 
		     (struct sockaddr *) &cli_addr, 
		     &clilen);
  if (newsockfd < 0) _error("ERROR on accept");
  debug_printf3("after accept: -> serverfd=%d newsockfd=%d\n",serverfd,newsockfd);
  
  /* add info to server fd data structure */
  connfd=get_new_fd_socket();
  if(connfd<0) return(-1);

  ldcs_socket_fdlist[connfd].type=LDCS_SOCKET_FD_TYPE_CONN;
  ldcs_socket_fdlist[connfd].fd=newsockfd;
  ldcs_socket_fdlist[connfd].server_fd=fd;

  ldcs_socket_fdlist[fd].conn_list_used++;
  if (ldcs_socket_fdlist[fd].conn_list_used > ldcs_socket_fdlist[fd].conn_list_size) {
    ldcs_socket_fdlist[fd].conn_list = realloc(ldcs_socket_fdlist[fd].conn_list, 
					     (ldcs_socket_fdlist[fd].conn_list_used + 15) * sizeof(int)
					     );
    ldcs_socket_fdlist[fd].conn_list_size = ldcs_socket_fdlist[fd].conn_list_used + 15;
  }
  ldcs_socket_fdlist[fd].conn_list[ldcs_socket_fdlist[fd].conn_list_used-1]=connfd;

  return(connfd);
};

int ldcs_open_server_connections_socket(int fd, int *more_avail) {
  *more_avail=0;
  return(ldcs_open_server_connection_socket(fd));
};

int ldcs_close_server_connection_socket(int fd) {
  int sockfd;
  if ((fd<0) || (fd>MAX_FD) )  _error("wrong fd");
  sockfd=ldcs_socket_fdlist[fd].fd;
  if(sockfd<0) return(-1);
  close(sockfd);
  return(0);
};

int ldcs_destroy_server_socket(int fd) {
  int serverfd;
  if ((fd<0) || (fd>MAX_FD) )  _error("wrong fd");
  serverfd=ldcs_socket_fdlist[fd].server_fd;
  if(serverfd<0) return(-1);
  close(serverfd);
  return(0);
};


/* ************************************************************** */
/* message transfer functions                                     */
/* ************************************************************** */
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

int ldcs_send_msg_socket(int fd, ldcs_message_t * msg) {

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

ldcs_message_t * ldcs_recv_msg_socket(int fd,  ldcs_read_block_t block) {
  ldcs_message_t *msg;
  char help[41];
  int n, connfd;
  if ((fd<0) || (fd>MAX_FD) )  _error("wrong fd");
  connfd=ldcs_socket_fdlist[fd].fd;

  msg = (ldcs_message_t *) malloc(sizeof(ldcs_message_t));
  if (!msg)  _error("could not allocate memory for message");
  

  n = _ldcs_read_socket(connfd,&msg->header,sizeof(msg->header), block);
  if (n == 0) {
    free(msg);
    return(NULL);
  }
  if (n < 0) _error("ERROR reading header from socket");

  if(msg->header.len>0) {

    msg->data = (char *) malloc(msg->header.len);
    if (!msg)  _error("could not allocate memory for message data");
    
    n = _ldcs_read_socket(connfd,msg->data,msg->header.len, LDCS_READ_BLOCK);
    if (n < 0) _error("ERROR reading message data from socket");
    if (n != msg->header.len) _error("received different number of bytes for message data");

  } else {
    msg->data = NULL;
  }

  bzero(help,41);if(msg->data) strncpy(help,msg->data,40);
  debug_printf3("received message of type: %s len=%d data=%s ...\n",
	       _message_type_to_str(msg->header.type),
	       msg->header.len,help );
  
  return(msg);
}


int ldcs_recv_msg_static_socket(int fd, ldcs_message_t *msg,  ldcs_read_block_t block) {
  char help[41];
  int rc=0;
  int n, connfd;
  if ((fd<0) || (fd>MAX_FD) )  _error("wrong fd");
  connfd=ldcs_socket_fdlist[fd].fd;


  n = _ldcs_read_socket(connfd,&msg->header,sizeof(msg->header), block);
  if (n == 0) return(rc);
  if (n < 0) _error("ERROR reading header from socket");

  if(msg->header.len>0) {

    msg->data = (char *) malloc(msg->header.len);
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


