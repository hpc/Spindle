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

#if !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <sys/syscall.h>
#include <assert.h>

#include "ldcs_api.h"
#include "ldcs_api_pipe.h"
#include "ldcs_api_pipe_notify.h"


/* ************************************************************** */
/* FD list                                                        */
/* ************************************************************** */


#define FDLIST_INITIAL_SIZE 32
static struct fdlist_entry_t *fdlist_pipe = NULL;
static int fdlist_pipe_cnt = 0;
static int fdlist_pipe_size = 0;

int get_new_fd_pipe()
{
   int i;
   if (fdlist_pipe_cnt == fdlist_pipe_size) {
      fdlist_pipe_size = fdlist_pipe_size ? fdlist_pipe_size * 2 : FDLIST_INITIAL_SIZE;
      fdlist_pipe = realloc(fdlist_pipe, fdlist_pipe_size * sizeof(struct fdlist_entry_t));
      if (!fdlist_pipe) {
         err_printf("Failed to allocate fdlist_pipe of size %d\n", fdlist_pipe_size);
         assert(0);
      }
      for (i = fdlist_pipe_cnt; i < fdlist_pipe_size; i++) {
         fdlist_pipe[i].inuse = 0;
      }
   }
   for (i = 0; i < fdlist_pipe_size; i++) {
      if (!fdlist_pipe[i].inuse) {
         fdlist_pipe[i].inuse = 1;
         fdlist_pipe_cnt++;
         return i;
      }
   }
   err_printf("Should have found empty fd, but didn't.");
   assert(0);
   return -1;
}

void free_fd_pipe (int fd) {
    fdlist_pipe[fd].inuse = 0;
    fdlist_pipe_cnt--;
    if (fdlist_pipe[fd].in_fn)
       free(fdlist_pipe[fd].in_fn);
    if (fdlist_pipe[fd].out_fn)
       free(fdlist_pipe[fd].out_fn);
}

int ldcs_get_fd_pipe (int fd) {
  int realfd=-1;
  if ((fd<0) || (fd>fdlist_pipe_size) )  _error("wrong fd");
  if(fdlist_pipe[fd].inuse) {
    if(fdlist_pipe[fd].type==LDCS_PIPE_FD_TYPE_SERVER) {
      realfd=ldcs_notify_get_fd(fdlist_pipe[fd].notify_fd);
    }
    if(fdlist_pipe[fd].type==LDCS_PIPE_FD_TYPE_CONN) {
      realfd=fdlist_pipe[fd].in_fd;
    }
  }
  return(realfd);
}
/* end of fd list */

int ldcs_create_server_pipe(char* location, int number) {
  int fd;
  struct stat st;

  fd=get_new_fd_pipe();
  if(fd<0) return(-1);

  int len = strlen(location) + 32;
  char *staging_dir = (char *) malloc(len);
  snprintf(staging_dir, len, "%s/spindle_comm.%d", location, number);

  debug_printf3("test direcrory before mkdir %s\n",staging_dir);
  if (stat(staging_dir, &st) == -1) {
    /* try create directory */
    if (-1 == mkdir(staging_dir, 0766)) {
      printf("mkdir: ERROR during mkdir %s\n", staging_dir);
      _error("mkdir failed");
    }
    debug_printf3("after mkdir %s\n",staging_dir);
  } else {
    if(S_ISDIR(st.st_mode)) {
      debug_printf3("%s already exists, using this direcory\n",staging_dir);
    } else {
      printf("mkdir: ERROR %s exists and is not a directory\n", staging_dir);
      _error("mkdir failed");
    }
  }

  /* FiFos will be created by client */
  /* -> setup notify to get new connections */
  fdlist_pipe[fd].type=LDCS_PIPE_FD_TYPE_SERVER;
  fdlist_pipe[fd].notify_fd=ldcs_notify_init(staging_dir);
  fdlist_pipe[fd].conn_list=NULL;
  fdlist_pipe[fd].conn_list_size=0;
  fdlist_pipe[fd].conn_list_used=0;
  fdlist_pipe[fd].path=staging_dir;
  fdlist_pipe[fd].in_fn=NULL;
  fdlist_pipe[fd].out_fn=NULL;
  
  char path[MAX_PATH_LEN];
  snprintf(path, MAX_PATH_LEN, "%s/ready", staging_dir);
  int readyfd = creat(path, 0600);
  close(readyfd);

  return(fd);
}

int ldcs_open_server_connection_pipe(int fd) {
  /*  */
  return(-1);
}

int ldcs_open_server_connections_pipe(int fd, int *more_avail) {
  int  fifoid, inout, connfd;
  char fifo[MAX_PATH_LEN];
  char *fifo_file;
  int  pid;
  
  if ((fd<0) || (fd>fdlist_pipe_size) )  _error("wrong fd");
  

  /* wait until a <pid>-1 fifo is created */
  for (;;) {
    fifo_file=ldcs_notify_get_next_file(fdlist_pipe[fd].notify_fd);
    if(fifo_file) {
       inout = 0; pid = 0;
       sscanf(fifo_file, "fifo-%d-%d", &pid, &inout);
       free(fifo_file);
       if(inout==1) {
          break;
       }
    }
  }

  connfd=get_new_fd_pipe();
  if(connfd<0) return(-1);
  fdlist_pipe[connfd].serverfd=fd;
  fdlist_pipe[connfd].type=LDCS_PIPE_FD_TYPE_CONN;

  sprintf(fifo, "%s/fifo-%d-0", fdlist_pipe[fd].path , pid );
  fdlist_pipe[connfd].out_fn = strdup(fifo);

  sprintf(fifo, "%s/fifo-%d-1", fdlist_pipe[fd].path , pid );
  fdlist_pipe[connfd].in_fn = strdup(fifo);

  debug_printf3("before open fifo '%s'\n",fdlist_pipe[connfd].out_fn);
  if (-1 == (fifoid = open(fdlist_pipe[connfd].out_fn, O_WRONLY))) _error("open fifo failed");
  debug_printf3("after open fifo (out): -> fifoid=%d\n",fifoid);
  fdlist_pipe[connfd].out_fd=fifoid;

  debug_printf3("before open fifo '%s'\n",fdlist_pipe[connfd].in_fn);
  if (-1 == (fifoid = open(fdlist_pipe[connfd].in_fn, O_RDONLY|O_NONBLOCK)))  _error("open fifo failed");
  /* if (-1 == (fifoid = open(fdlist_pipe[connfd].in_fn, O_RDONLY)))  _error("open fifo failed"); */
  debug_printf3("after open fifo (in) : -> fifoid=%d\n",fifoid);
  fdlist_pipe[connfd].in_fd=fifoid;

  /* add info to server fd data structure */
  fdlist_pipe[fd].conn_list_used++;
  if (fdlist_pipe[fd].conn_list_used > fdlist_pipe[fd].conn_list_size) {
    fdlist_pipe[fd].conn_list = realloc(fdlist_pipe[fd].conn_list, 
					     (fdlist_pipe[fd].conn_list_used + 15) * sizeof(int)
					     );
    fdlist_pipe[fd].conn_list_size = fdlist_pipe[fd].conn_list_used + 15;
  }
  fdlist_pipe[fd].conn_list[fdlist_pipe[fd].conn_list_used-1]=connfd;

  *more_avail=ldcs_notify_more_avail(fdlist_pipe[fd].notify_fd);

  return(connfd);
};

int ldcs_close_server_connection_pipe(int fd) {
  int rc=0, serverfd, c;

  if ((fd<0) || (fd>fdlist_pipe_size) )  _error("wrong fd");
  
  debug_printf3(" closing fd %d for conn %d, closing connection\n",fdlist_pipe[fd].in_fd,fd);
  close(fdlist_pipe[fd].in_fd);
  debug_printf3(" closing fd %d for conn %d, closing connection\n",fdlist_pipe[fd].out_fd,fd);
  close(fdlist_pipe[fd].out_fd);

  /* remove connection from server list */
  serverfd=fdlist_pipe[fd].serverfd;
  for(c=0;c<fdlist_pipe[serverfd].conn_list_used;c++) {
    if(fdlist_pipe[serverfd].conn_list[c]==fd) {
      fdlist_pipe[serverfd].conn_list[c]=-1;
    }
  }

  free_fd_pipe(fd);

  return(rc);
};

int ldcs_destroy_server_pipe(int fd) {

  int rc=0;
  char path[MAX_PATH_LEN];

  if ((fd<0) || (fd>fdlist_pipe_size) )  _error("wrong fd");
  
  ldcs_notify_destroy(fdlist_pipe[fd].notify_fd);

  snprintf(path, MAX_PATH_LEN, "%s/ready", fdlist_pipe[fd].path);
  unlink(path);
  rmdir(fdlist_pipe[fd].path);
  free_fd_pipe(fd);

  return(rc);
};

/* ************************************************************** */
/* message transfer functions                                     */
/* ************************************************************** */
int ldcs_send_msg_pipe(int fd, ldcs_message_t * msg) {

  int n;

  if ((fd<0) || (fd>fdlist_pipe_size) )  _error("wrong fd");
  
  debug_printf3("sending message of type: %s len=%d data=%s ...\n",
	       _message_type_to_str(msg->header.type),
	       msg->header.len,msg->data );  

  n = _ldcs_write_pipe(fdlist_pipe[fd].out_fd,&msg->header,sizeof(msg->header));
  if (n < 0) _error("ERROR writing header to pipe");

  if(msg->header.len>0) {
    n = _ldcs_write_pipe(fdlist_pipe[fd].out_fd,(void *) msg->data,msg->header.len);
    if (n < 0) _error("ERROR writing data to pipe");
    if (n != msg->header.len) _error("sent different number of bytes for message data");
  }
    
  return(0);
}

ldcs_message_t * ldcs_recv_msg_pipe(int fd, ldcs_read_block_t block ) {
  ldcs_message_t *msg;
  int n;

  if ((fd<0) || (fd>fdlist_pipe_size) )  _error("wrong fd");

  msg = (ldcs_message_t *) malloc(sizeof(ldcs_message_t));
  if (!msg)  _error("could not allocate memory for message");

  n = _ldcs_read_pipe(fdlist_pipe[fd].in_fd,&msg->header,sizeof(msg->header), block);
  if (n == 0) {
    free(msg);
    return(NULL);
  }
  if (n < 0) _error("ERROR reading header from connection");

  if(msg->header.len>0) {

    msg->data = (char *) malloc(sizeof(msg->header.len));
    if (!msg)  _error("could not allocate memory for message data");

    n = _ldcs_read_pipe(fdlist_pipe[fd].in_fd,msg->data,msg->header.len, LDCS_READ_BLOCK);
    if (n < 0) _error("ERROR reading message data from socket");
    if (n != msg->header.len) _error("received different number of bytes for message data");

  } else {
    msg->data = NULL;
  }

  debug_printf3("received message of type: %s len=%d data=%s ...\n",
	       _message_type_to_str(msg->header.type),
	       msg->header.len, msg->data );

  return(msg);
}

int ldcs_recv_msg_static_pipe(int fd, ldcs_message_t *msg, ldcs_read_block_t block) {
  int n;
  int rc=0;
  msg->header.type=LDCS_MSG_UNKNOWN;
  msg->header.len=0;
  if ((fd<0) || (fd>fdlist_pipe_size) )  _error("wrong fd");

  n = _ldcs_read_pipe(fdlist_pipe[fd].in_fd,&msg->header,sizeof(msg->header), block);
  if (n == 0) {
     /* Disconnect.  Return an artificial client end message */
     debug_printf2("Client disconnected.  Returning END message\n");
     msg->header.type = LDCS_MSG_END;
     msg->header.len = 0;
     msg->data = NULL;
     return(rc);
  }
  if (n < 0) _error("ERROR reading header from connection");

  if(msg->header.len>0) {
    n = _ldcs_read_pipe(fdlist_pipe[fd].in_fd,msg->data,msg->header.len, LDCS_READ_BLOCK);
    if (n == 0) return(rc);
    if (n < 0) _error("ERROR reading message data from socket");
    if (n != msg->header.len) _error("received different number of bytes for message data");

  } else {
    *msg->data = '\0';
  }

  debug_printf3("received message of type: %s len=%d data=%s ...\n",
	       _message_type_to_str(msg->header.type),
	       msg->header.len, msg->data );

  return(rc);
}


int _ldcs_read_pipe(int fd, void *data, int bytes, ldcs_read_block_t block ) {

  int         left,bsumread;
  ssize_t     btoread, bread;
  char       *dataptr;
  
  left      = bytes;
  bsumread  = 0;
  dataptr   = (char*) data;
  bread     = 0;

  while (left > 0)  {
    btoread    = left;
    debug_printf3("before read from fifo %d, bytes_to_read = %ld\n", fd, btoread);
    bread      = read(fd, dataptr, btoread);
    if(bread<0) {
       if( (errno==EAGAIN) || (errno==EWOULDBLOCK) ) {
          debug_printf3("read from fifo: got EAGAIN or EWOULDBLOCK\n");
          if(block==LDCS_READ_NO_BLOCK) return(0);
          else continue;
       } else { 
          debug_printf3("read from fifo: %ld bytes ... errno=%d (%s)\n",bread,errno,strerror(errno));
       }
    } else {
       debug_printf3("read from fifo: %ld bytes ...\n",bread);
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



int _ldcs_write_pipe(int fd, const void *data, int bytes ) {
  int         left,bsumwrote;
  ssize_t     bwrite, bwrote;
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

