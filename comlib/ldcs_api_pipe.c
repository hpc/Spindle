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

#include "ldcs_api.h"
#include "ldcs_api_pipe.h"
#include "ldcs_api_pipe_notify.h"


/* ************************************************************** */
/* FD list                                                        */
/* ************************************************************** */
typedef enum {
   LDCS_PIPE_FD_TYPE_SERVER,
   LDCS_PIPE_FD_TYPE_CONN,
   LDCS_CLIENT_STATUS_UNKNOWN
} fd_list_entry_type_t;

#define MAX_FD 100
struct fdlist_entry_t
{
  int   inuse;
  fd_list_entry_type_t type;

  /* server part */
  int   notify_fd; 
  int   conn_list_size; 
  int   conn_list_used; 
  int  *conn_list; 
  char *path; 

  /* connection part */
  int   in_fd;
  char *in_fn;
  int   out_fd;
  char *out_fn;
  int   serverfd;
};

struct fdlist_entry_t ldcs_pipe_fdlist[MAX_FD];
int ldcs_fdlist_pipe_cnt=-1;

int get_new_fd_pipe () {
  int fd;
  if(ldcs_fdlist_pipe_cnt==-1) {
    /* init fd list */
    for(fd=0;fd<MAX_FD;fd++) ldcs_pipe_fdlist[fd].inuse=0;
  }
  if(ldcs_fdlist_pipe_cnt+1<MAX_FD) {

    fd=0;
    while ( (fd<MAX_FD) && (ldcs_pipe_fdlist[fd].inuse==1) ) fd++;
    ldcs_pipe_fdlist[fd].inuse=1;
    ldcs_fdlist_pipe_cnt++;
    return(fd);
  } else {
    return(-1);
  }
}

void free_fd_pipe (int fd) {
    ldcs_pipe_fdlist[fd].inuse=0;
    ldcs_fdlist_pipe_cnt--;
    free(ldcs_pipe_fdlist[fd].in_fn);
    free(ldcs_pipe_fdlist[fd].out_fn);
}

int ldcs_get_fd_pipe (int fd) {
  int realfd=-1;
  if ((fd<0) || (fd>MAX_FD) )  _error("wrong fd");
  if(ldcs_pipe_fdlist[fd].inuse) {
    if(ldcs_pipe_fdlist[fd].type==LDCS_PIPE_FD_TYPE_SERVER) {
      realfd=ldcs_notify_get_fd(ldcs_pipe_fdlist[fd].notify_fd);
    }
    if(ldcs_pipe_fdlist[fd].type==LDCS_PIPE_FD_TYPE_CONN) {
      realfd=ldcs_pipe_fdlist[fd].in_fd;
    }
  }
  return(realfd);
}
/* end of fd list */

int ldcs_open_connection_pipe(char* location, int number) {
  int fd, fifoid;
  struct stat st;
  int stat_cnt;
  char fifo[MAX_PATH_LEN];

  fd=get_new_fd_pipe();
  if(fd<0) return(-1);
  
  ldcs_pipe_fdlist[fd].type=LDCS_PIPE_FD_TYPE_CONN;

  /* don't use the number, use instead the pid  */
  number=getpid();

  /* TBD: remove pipe if already existing */
  
  /* wait for directory (at most one minute) */
  stat_cnt=0;
  while ((stat(location, &st) == -1) && (stat_cnt<60)) {
    debug_printf("waiting: location %s does not exists (after %d seconds)\n",location,stat_cnt);
    sleep(1);
    stat_cnt++;
  }
  

  /* create incomming fifo */
  sprintf(fifo, "%s/fifo-%d-0", location , number );
  _ldcs_mkfifo(fifo);
  debug_printf("after make fifo: -> path=%s\n",fifo);
  if (NULL == (ldcs_pipe_fdlist[fd].in_fn = (char *) malloc(strlen(fifo)+1))) return -1;
  strcpy(ldcs_pipe_fdlist[fd].in_fn,fifo);

  /* create outgoing fifo */
  sprintf(fifo, "%s/fifo-%d-1", location , number );
  _ldcs_mkfifo(fifo);
  debug_printf("after make fifo: -> path=%s\n",fifo);  
  if (NULL == (ldcs_pipe_fdlist[fd].out_fn = (char *) malloc(strlen(fifo)+1))) return -1;
  strcpy(ldcs_pipe_fdlist[fd].out_fn,fifo);

  /* open incomming fifo */
  /* |O_NONBLOCK not for client */
  debug_printf("before open fifo %s\n",ldcs_pipe_fdlist[fd].in_fn);
  if (-1 == (fifoid = open(ldcs_pipe_fdlist[fd].in_fn, O_RDONLY))) _error("open fifo failed");
  debug_printf("after  open fifo: -> fifoid=%d\n",fifoid);
  ldcs_pipe_fdlist[fd].in_fd=fifoid;

  /* open outgoing fifo */
  debug_printf("before open fifo %s\n",ldcs_pipe_fdlist[fd].out_fn);
  if (-1 == (fifoid = open(ldcs_pipe_fdlist[fd].out_fn, O_WRONLY))) _error("open fifo failed");
  debug_printf("after open fifo: -> fifoid=%d\n",fifoid);
  ldcs_pipe_fdlist[fd].out_fd=fifoid;



  return(fd);
  
}

int ldcs_close_connection_pipe(int fd) {
  int rc=0;
  struct stat st;
  if ((fd<0) || (fd>MAX_FD) )  _error("wrong fd");
  
  rc=close(ldcs_pipe_fdlist[fd].in_fd);
  if(rc!=0) {
    debug_printf("error while closing fifo %s errno=%d (%s)\n",ldcs_pipe_fdlist[fd].in_fn,errno,strerror(errno));
  }
  rc=close(ldcs_pipe_fdlist[fd].out_fd);
  if(rc!=0) {
    debug_printf("error while closing fifo %s errno=%d (%s)\n",ldcs_pipe_fdlist[fd].out_fn,errno,strerror(errno));
  }

  if (stat(ldcs_pipe_fdlist[fd].in_fn, &st)) _error("stat failed");
  if(S_ISFIFO(st.st_mode)) {
    rc=unlink(ldcs_pipe_fdlist[fd].in_fn); 
    if(rc!=0) {
      debug_printf("error while unlink fifo %s errno=%d (%s)\n",ldcs_pipe_fdlist[fd].in_fn,errno,strerror(errno));
    }
    debug_printf("unlink: %s\n",ldcs_pipe_fdlist[fd].in_fn);
  }

  if (stat(ldcs_pipe_fdlist[fd].out_fn, &st)) _error("stat failed");
  if(S_ISFIFO(st.st_mode)) {
    rc=unlink(ldcs_pipe_fdlist[fd].out_fn);
    if(rc!=0) {
      debug_printf("error while unlink fifo %s errno=%d (%s)\n",ldcs_pipe_fdlist[fd].out_fn,errno,strerror(errno));
    }
    debug_printf("unlink: %s\n",ldcs_pipe_fdlist[fd].in_fn);
  }

  free_fd_pipe(fd);
  
  return(rc);
}


int ldcs_create_server_pipe(char* location, int number) {
  int fd;
  struct stat st;

  fd=get_new_fd_pipe();
  if(fd<0) return(-1);

  debug_printf("test direcrory before mkdir %s\n",location);
  if (stat(location, &st) == -1) {
    /* try create directory */
    if (-1 == mkdir(location, 0766)) {
      printf("mkdir: ERROR during mkdir %s\n", location);
      _error("mkdir failed");
    }
    debug_printf("after mkdir %s\n",location);
  } else {
    if(S_ISDIR(st.st_mode)) {
      debug_printf("%s already exists, using this direcory\n",location);
    } else {
      printf("mkdir: ERROR %s exists and is not a directory\n", location);
      _error("mkdir failed");
    }
  }

  /* FiFos will be created by client */
  /* -> setup notify to get new connections */
  ldcs_pipe_fdlist[fd].type=LDCS_PIPE_FD_TYPE_SERVER;
  ldcs_pipe_fdlist[fd].notify_fd=ldcs_notify_init(location);
  ldcs_pipe_fdlist[fd].conn_list=NULL;
  ldcs_pipe_fdlist[fd].conn_list_size=0;
  ldcs_pipe_fdlist[fd].conn_list_used=0;
  ldcs_pipe_fdlist[fd].path=strdup(location);
  
  return(fd);
}

int ldcs_open_server_connection_pipe(int fd) {
  /*  */
  return(-1);
}

int ldcs_open_server_connections_pipe(int fd, int *more_avail) {
  int  fifoid, finished, inout, connfd;
  char fifo[MAX_PATH_LEN];
  char *fifo_file;
  int  pid;
  
  if ((fd<0) || (fd>MAX_FD) )  _error("wrong fd");
  
  finished=0;

  /* wait until a <pid>-1 fifo is created */
  while(!finished) {
    fifo_file=ldcs_notify_get_next_file(ldcs_pipe_fdlist[fd].notify_fd);
    if(fifo_file) {
      sscanf(fifo_file, "fifo-%d-%d", &pid, &inout);
      if(inout==1) {
	finished=1;
      }
      free(fifo_file);
    }
  }

  connfd=get_new_fd_pipe();
  if(connfd<0) return(-1);
  ldcs_pipe_fdlist[connfd].serverfd=fd;
  ldcs_pipe_fdlist[connfd].type=LDCS_PIPE_FD_TYPE_CONN;

  sprintf(fifo, "%s/fifo-%d-0", ldcs_pipe_fdlist[fd].path , pid );
  ldcs_pipe_fdlist[connfd].out_fn = strdup(fifo);

  sprintf(fifo, "%s/fifo-%d-1", ldcs_pipe_fdlist[fd].path , pid );
  ldcs_pipe_fdlist[connfd].in_fn = strdup(fifo);

  debug_printf("before open fifo '%s'\n",ldcs_pipe_fdlist[connfd].out_fn);
  if (-1 == (fifoid = open(ldcs_pipe_fdlist[connfd].out_fn, O_WRONLY))) _error("open fifo failed");
  debug_printf("after open fifo (out): -> fifoid=%d\n",fifoid);
  ldcs_pipe_fdlist[connfd].out_fd=fifoid;

  debug_printf("before open fifo '%s'\n",ldcs_pipe_fdlist[connfd].in_fn);
  if (-1 == (fifoid = open(ldcs_pipe_fdlist[connfd].in_fn, O_RDONLY|O_NONBLOCK)))  _error("open fifo failed");
  /* if (-1 == (fifoid = open(ldcs_pipe_fdlist[connfd].in_fn, O_RDONLY)))  _error("open fifo failed"); */
  debug_printf("after open fifo (in) : -> fifoid=%d\n",fifoid);
  ldcs_pipe_fdlist[connfd].in_fd=fifoid;

  /* add info to server fd data structure */
  ldcs_pipe_fdlist[fd].conn_list_used++;
  if (ldcs_pipe_fdlist[fd].conn_list_used > ldcs_pipe_fdlist[fd].conn_list_size) {
    ldcs_pipe_fdlist[fd].conn_list = realloc(ldcs_pipe_fdlist[fd].conn_list, 
					     (ldcs_pipe_fdlist[fd].conn_list_used + 15) * sizeof(int)
					     );
    ldcs_pipe_fdlist[fd].conn_list_size = ldcs_pipe_fdlist[fd].conn_list_used + 15;
  }
  ldcs_pipe_fdlist[fd].conn_list[ldcs_pipe_fdlist[fd].conn_list_used-1]=connfd;

  *more_avail=ldcs_notify_more_avail(ldcs_pipe_fdlist[fd].notify_fd);

  return(connfd);
};

int ldcs_close_server_connection_pipe(int fd) {
  int rc=0, serverfd, c;

  if ((fd<0) || (fd>MAX_FD) )  _error("wrong fd");
  
  debug_printf(" closing fd %d for conn %d, closing connection\n",ldcs_pipe_fdlist[fd].in_fd,fd);
  close(ldcs_pipe_fdlist[fd].in_fd);
  debug_printf(" closing fd %d for conn %d, closing connection\n",ldcs_pipe_fdlist[fd].out_fd,fd);
  close(ldcs_pipe_fdlist[fd].out_fd);

  /* remove connection from server list */
  serverfd=ldcs_pipe_fdlist[fd].serverfd;
  for(c=0;c<ldcs_pipe_fdlist[serverfd].conn_list_used;c++) {
    if(ldcs_pipe_fdlist[serverfd].conn_list[c]==fd) {
      ldcs_pipe_fdlist[serverfd].conn_list[c]=-1;
    }
  }

  free_fd_pipe(fd);

  return(rc);
};

int ldcs_destroy_server_pipe(int fd) {

  int rc=0;

  if ((fd<0) || (fd>MAX_FD) )  _error("wrong fd");
  
  ldcs_notify_destroy(ldcs_pipe_fdlist[fd].notify_fd);
  

  free_fd_pipe(fd);

  return(rc);
};

/* ************************************************************** */
/* message transfer functions                                     */
/* ************************************************************** */
int ldcs_send_msg_pipe(int fd, ldcs_message_t * msg) {

  int n;

  if ((fd<0) || (fd>MAX_FD) )  _error("wrong fd");
  
  debug_printf("sending message of type: %s len=%d data=%s ...\n",
	       _message_type_to_str(msg->header.type),
	       msg->header.len,msg->data );  

  n = _ldcs_write_pipe(ldcs_pipe_fdlist[fd].out_fd,&msg->header,sizeof(msg->header));
  if (n < 0) _error("ERROR writing header to pipe");

  if(msg->header.len>0) {
    n = _ldcs_write_pipe(ldcs_pipe_fdlist[fd].out_fd,(void *) msg->data,msg->header.len);
    if (n < 0) _error("ERROR writing data to pipe");
    if (n != msg->header.len) _error("sent different number of bytes for message data");
  }
    
  return(0);
}

ldcs_message_t * ldcs_recv_msg_pipe(int fd, ldcs_read_block_t block ) {
  ldcs_message_t *msg;
  int n;

  if ((fd<0) || (fd>MAX_FD) )  _error("wrong fd");

  msg = (ldcs_message_t *) malloc(sizeof(ldcs_message_t));
  if (!msg)  _error("could not allocate memory for message");

  n = _ldcs_read_pipe(ldcs_pipe_fdlist[fd].in_fd,&msg->header,sizeof(msg->header), block);
  if (n == 0) {
    free(msg);
    return(NULL);
  }
  if (n < 0) _error("ERROR reading header from connection");

  if(msg->header.len>0) {

    msg->data = (char *) malloc(sizeof(msg->header.len));
    if (!msg)  _error("could not allocate memory for message data");
    msg->alloclen=msg->header.len;    

    n = _ldcs_read_pipe(ldcs_pipe_fdlist[fd].in_fd,msg->data,msg->header.len, LDCS_READ_BLOCK);
    if (n < 0) _error("ERROR reading message data from socket");
    if (n != msg->header.len) _error("received different number of bytes for message data");

  } else {
    msg->data = NULL;
  }

  debug_printf("received message of type: %s len=%d data=%s ...\n",
	       _message_type_to_str(msg->header.type),
	       msg->header.len, msg->data );

  return(msg);
}


int ldcs_recv_msg_static_pipe(int fd, ldcs_message_t *msg, ldcs_read_block_t block) {
  int n;
  int rc=0;
  msg->header.type=LDCS_MSG_UNKNOWN;
  msg->header.len=0;
  if ((fd<0) || (fd>MAX_FD) )  _error("wrong fd");

  n = _ldcs_read_pipe(ldcs_pipe_fdlist[fd].in_fd,&msg->header,sizeof(msg->header), block);
  if (n == 0) return(rc);
  if (n < 0) _error("ERROR reading header from connection");

  if(msg->header.len>msg->alloclen) {
    debug_printf("ERROR message too long: %s len=%d ...\n",
		 _message_type_to_str(msg->header.type),msg->header.len );
    _error("ERROR message too long");
  }

  if(msg->header.len>0) {
    bzero(msg->data,msg->alloclen);
    n = _ldcs_read_pipe(ldcs_pipe_fdlist[fd].in_fd,msg->data,msg->header.len, LDCS_READ_BLOCK);
    if (n == 0) return(rc);
    if (n < 0) _error("ERROR reading message data from socket");
    if (n != msg->header.len) _error("received different number of bytes for message data");

  } else {
    *msg->data = '\0';
  }

  debug_printf("received message of type: %s len=%d data=%s ...\n",
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
    debug_printf("before read from fifo %d \n",fd);
    bread      = read(fd, dataptr, btoread);
    if(bread<0) {
      if( (errno==EAGAIN) || (errno==EWOULDBLOCK) ) {
	debug_printf("read from fifo: got EAGAIN or EWOULDBLOCK\n");
	if(block==LDCS_READ_NO_BLOCK) return(0);
	else continue;
      } else { 
	debug_printf("read from fifo: %d bytes ... errno=%d (%s)\n",bread,errno,strerror(errno));
      }
    } else {
      debug_printf("read from fifo: %d bytes ...\n",bread);
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

int _ldcs_mkfifo(char *fifo) {
  int rc=0;
  
  debug_printf("mkfifo: %s \n", fifo);
  rc=mkfifo(fifo, 0666);
  debug_printf("mkfifo: %s --> %d (%d,%s)\n", fifo,rc,errno,strerror(errno));
    
  if (rc == -1) {
    if(errno==EEXIST) {
      char buffer[MAX_PATH_LEN];
      rc=gethostname(buffer, MAX_PATH_LEN);
      printf("mkfifo: Warning fifo is already been created %s on host %s, trying to use this\n", fifo, buffer);
    } else {
      char buffer[MAX_PATH_LEN];
      rc=gethostname(buffer, MAX_PATH_LEN);
      printf("mkfifo: ERROR during mkfifo %s on host %s, exiting  errno=%d (%s) \n", 
	     fifo, buffer,errno,strerror(errno));
     _error("mkfifo failed");
    }
  }

  return(rc);
}
