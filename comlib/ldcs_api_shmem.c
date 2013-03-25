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
#include "ldcs_api_shmem.h"

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#define	FILE_MODE	(S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)
#include <fcntl.h>           /* For O_* constants */
#include <sys/stat.h>        /* For mode constants */
#include <semaphore.h>

#define MAX_MSG_SIZE 256
#define MAX_NUM_CLIENTS 64

int _ldcs_init_datastruct_shmem(int fd);
int _ldcs_mangle_name_shmem(char *mname);
char* _ldcs_get_shmem_name(char* name);
int ldcs_create_server_or_client_shmem(char* location, int number);





/* ************************************************************** */
/* SHMEM data structure                                           */
/* ************************************************************** */
struct ldcs_shmem_data_struct {
  int    c_clients;
  sem_t *c_in_sem[MAX_NUM_CLIENTS];
  sem_t *c_out_sem[MAX_NUM_CLIENTS];
  int    conn_state[MAX_NUM_CLIENTS];  
  int    in_msg_size[MAX_NUM_CLIENTS];  
  int    out_msg_size[MAX_NUM_CLIENTS];  
  char   in_buffer[MAX_NUM_CLIENTS*MAX_MSG_SIZE]; 
  char   out_buffer[MAX_NUM_CLIENTS*MAX_MSG_SIZE]; 
};
typedef struct ldcs_shmem_data_struct ldcs_shmem_data_t;

/* ************************************************************** */
/* FD list                                                        */
/* ************************************************************** */
typedef enum {
   LDCS_SHMEM_FD_TYPE_SERVER,
   LDCS_SHMEM_FD_TYPE_CONN,
   LDCS_SHMEM_FD_TYPE_UNKNOWN
} fd_list_entry_type_t;

#define MAX_FD 100
struct fdlist_entry_t
{
  int   inuse;
  fd_list_entry_type_t type;

  /* general attributes */
  char   base_name[MAX_PATH_LEN];

  char   sem_decide_name[MAX_PATH_LEN];
  sem_t *sem_decide;

  char   sem_lock_name[MAX_PATH_LEN];
  sem_t *sem_lock;

  char   sem_init_name[MAX_PATH_LEN];
  sem_t *sem_init;

  char   shmfile_name[MAX_PATH_LEN];
  int    shmemfd;
  ldcs_shmem_data_t * shmem_data;

  /* server part */
  int   server_fd; 
  int   conn_list_size; 
  int   conn_list_used; 
  int  *conn_list; 

  /* connection part */
  int   myclientnr;
  int   serverid;
};

struct fdlist_entry_t ldcs_shmem_fdlist[MAX_FD];
int ldcs_fdlist_shmem_cnt=-1;

int get_new_fd_shmem () {
  int fd;
  if(ldcs_fdlist_shmem_cnt==-1) {
    /* init fd list */
    for(fd=0;fd<MAX_FD;fd++) ldcs_shmem_fdlist[fd].inuse=0;
  }
  if(ldcs_fdlist_shmem_cnt+1<MAX_FD) {

    fd=0;
    while ( (fd<MAX_FD) && (ldcs_shmem_fdlist[fd].inuse==1) ) fd++;
    ldcs_shmem_fdlist[fd].inuse=1;
    ldcs_fdlist_shmem_cnt++;
    return(fd);
  } else {
    return(-1);
  }
}

void free_fd_shmem (int fd) {
  ldcs_shmem_fdlist[fd].inuse=0;
  ldcs_fdlist_shmem_cnt--;
}

/* end of fd list */


int ldcs_get_fd_shmem (int fd) {
  int realfd=-1;
  if ((fd<0) || (fd>MAX_FD) )  _error("wrong fd");
  if(ldcs_shmem_fdlist[fd].inuse) {
    
    /* no implementation if selectable fd for shmem  */
    
  }
  return(realfd);
}



/* ************************************************************** */
/* CLIENT functions                                               */
/* ************************************************************** */

int ldcs_open_connection_shmem(char* hostname, int portno) {
  int fd;

  fd=get_new_fd_shmem();
  if(fd<0) return(-1);

  ldcs_shmem_fdlist[fd].type=LDCS_SHMEM_FD_TYPE_CONN;
  
  _ldcs_init_datastruct_shmem(fd);

  debug_printf3("after connect: -> port=%d\n",portno);
 
  return(fd);
  
}

char *ldcs_get_connection_string_shmem(int fd)
{
   return "";
}

int ldcs_register_connection_shmem(char *connection_str)
{
   return ldcs_open_connection_shmem("", 0);
}

int ldcs_close_connection_shmem(int fd) {
  int rc=0;

  if ((fd<0) || (fd>MAX_FD) )  _error("wrong fd");

  return(rc);
}

/* ************************************************************** */
/* SERVER functions                                               */
/* ************************************************************** */


int ldcs_create_server_shmem(char* location, int number) {
  int fd;

  fd=get_new_fd_shmem();
  if(fd<0) return(-1);

  ldcs_shmem_fdlist[fd].type=LDCS_SHMEM_FD_TYPE_SERVER;
  ldcs_shmem_fdlist[fd].sem_decide=NULL;

  _ldcs_init_datastruct_shmem(fd);

  ldcs_shmem_fdlist[fd].conn_list=NULL;
  ldcs_shmem_fdlist[fd].conn_list_size=0;
  ldcs_shmem_fdlist[fd].conn_list_used=0;

  return(fd);
}

int ldcs_open_server_connection_shmem(int fd) {
  int connfd=-1;

  if ((fd<0) || (fd>MAX_FD) )  _error("wrong fd");
  /* please use function for multiple new connection, see below */
  return(connfd);
};

int ldcs_open_server_connections_shmem(int fd, int *more_avail) {
  int connfd=-1;

  if ((fd<0) || (fd>MAX_FD) )  _error("wrong fd");

  /* scan for new connection */

  *more_avail=0;
  return(connfd);
};

int ldcs_close_server_connection_shmem(int fd) {
  if ((fd<0) || (fd>MAX_FD) )  _error("wrong fd");

  if(ldcs_shmem_fdlist[fd].sem_decide) {
    sem_unlink(ldcs_shmem_fdlist[fd].sem_decide_name);
  }

  return(0);
};

int ldcs_destroy_server_shmem(int fd) {
  if ((fd<0) || (fd>MAX_FD) )  _error("wrong fd");
  
  if(ldcs_shmem_fdlist[fd].sem_decide) {
    sem_unlink(ldcs_shmem_fdlist[fd].sem_decide_name);
  }

  return(0);
};


/* ************************************************************** */
/* Additional Functions                                           */
/* ************************************************************** */

int _ldcs_init_datastruct_shmem(int fd) {
  int rc=0;
  if ((fd<0) || (fd>MAX_FD) )  _error("wrong fd");

  sprintf(ldcs_shmem_fdlist[fd].sem_lock_name,"lock-%s",ldcs_shmem_fdlist[fd].base_name);
  sprintf(ldcs_shmem_fdlist[fd].sem_init_name,"dsinit-%s",ldcs_shmem_fdlist[fd].base_name);
  sprintf(ldcs_shmem_fdlist[fd].shmfile_name,"file-%s",ldcs_shmem_fdlist[fd].base_name);

  /* get lock for data structure */
  ldcs_shmem_fdlist[fd].sem_lock=sem_open(ldcs_shmem_fdlist[fd].sem_lock_name, O_CREAT, FILE_MODE, 1);
  debug_printf3("after open sem ldcs_sem_lock %s %p errno(%d,%s)\n",ldcs_shmem_fdlist[fd].sem_lock_name,ldcs_shmem_fdlist[fd].sem_lock,errno,strerror(errno));
  sem_wait(ldcs_shmem_fdlist[fd].sem_lock);
  debug_printf3("after sem_wait(ldcs_sem_lock)\n");

  /* check if data structure is initialized */
  /* -->  get lock for init, init will not neccesary done by server  */
  ldcs_shmem_fdlist[fd].sem_init=sem_open(ldcs_shmem_fdlist[fd].sem_init_name, O_CREAT, FILE_MODE, 1);
  debug_printf3("after open sem ldcs_sem_init %s %p errno(%d,%s)\n",ldcs_shmem_fdlist[fd].sem_init_name,ldcs_shmem_fdlist[fd].sem_init,errno,strerror(errno));
  if(!sem_trywait(ldcs_shmem_fdlist[fd].sem_init)) {

    debug_printf3("open shmem file = %s + create\n",ldcs_shmem_fdlist[fd].shmfile_name);
    ldcs_shmem_fdlist[fd].shmemfd=shm_open(ldcs_shmem_fdlist[fd].shmfile_name, O_RDWR|O_CREAT, 0600);
    debug_printf3("after open fd=%d\n",ldcs_shmem_fdlist[fd].shmemfd);

    ftruncate(ldcs_shmem_fdlist[fd].shmemfd, sizeof(ldcs_shmem_data_t));
    debug_printf3("after ftruncate to %ld bytes\n",sizeof(ldcs_shmem_data_t));

    ldcs_shmem_fdlist[fd].shmem_data=mmap(NULL, sizeof(ldcs_shmem_data_t), PROT_READ | PROT_WRITE, MAP_SHARED, ldcs_shmem_fdlist[fd].shmemfd, 0);
    debug_printf3("after mmap: %p\n",ldcs_shmem_fdlist[fd].shmem_data);
    
    /* init data structure */
    ldcs_shmem_fdlist[fd].shmem_data->c_clients=0;

    /* release lock */
    sem_post(ldcs_shmem_fdlist[fd].sem_lock);

  } else {
    debug_printf3("open shmem file = %s\n",ldcs_shmem_fdlist[fd].shmfile_name);
    ldcs_shmem_fdlist[fd].shmemfd=shm_open(ldcs_shmem_fdlist[fd].shmfile_name, O_RDWR, 0600);
    debug_printf3("after open fd=%d\n",ldcs_shmem_fdlist[fd].shmemfd);

    ldcs_shmem_fdlist[fd].shmem_data=mmap(NULL, sizeof(ldcs_shmem_data_t), PROT_READ | PROT_WRITE, MAP_SHARED, ldcs_shmem_fdlist[fd].shmemfd, 0);
    debug_printf3("after mmap: %p\n",ldcs_shmem_fdlist[fd].shmem_data);
    
    /* release lock */
    sem_post(ldcs_shmem_fdlist[fd].sem_lock);
    
  }

  /* register as client */
  if(ldcs_shmem_fdlist[fd].type==LDCS_SHMEM_FD_TYPE_CONN) {
    ldcs_shmem_fdlist[fd].myclientnr=ldcs_shmem_fdlist[fd].shmem_data->c_clients;
    ldcs_shmem_fdlist[fd].shmem_data->c_clients++;
  }

  /* do further initialisation */

  return(rc);
};


int _ldcs_mangle_name_shmem(char *mname) {
  char *c;
  int rc=0;
  for(c=mname;*c!='\0';c++) {
    if(*c=='/') *c='_';
  }
  return(rc);
};


/* returns a name, containing the last MAX_PATH_LEN-20 mangled characters */
char* _ldcs_get_shmem_name(char* name) {
  char tmpname[MAX_PATH_LEN];
  int len=strlen(name);
  int offset=0;
  if(len>(MAX_PATH_LEN-20)) {
    offset=len-(MAX_PATH_LEN-20);
  }
  bzero(tmpname,MAX_PATH_LEN);
  strncpy(tmpname,name+offset,MAX_PATH_LEN-20);
  _ldcs_mangle_name_shmem(tmpname);
  return(strdup(tmpname));
}

/* needed for auditclient to auditclient communication */
int ldcs_create_server_or_client_shmem(char* location, int number) {
  int fd=-1;
  int iamserver;

  char *shmem_basename=_ldcs_get_shmem_name(location);

  fd=get_new_fd_shmem();
  if(fd<0) return(-1);

  strncpy(ldcs_shmem_fdlist[fd].base_name,shmem_basename,MAX_PATH_LEN);
  sprintf(ldcs_shmem_fdlist[fd].sem_decide_name,"decide-%s",ldcs_shmem_fdlist[fd].base_name);

  /* init semaphore for DECIDING WHO IS SERVER */ 
  ldcs_shmem_fdlist[fd].sem_decide=sem_open(ldcs_shmem_fdlist[fd].sem_decide_name, O_CREAT, FILE_MODE, 1);
  debug_printf3("after sem_open semname=%s %p errno(%d,%s)\n",ldcs_shmem_fdlist[fd].sem_decide_name, 
	       ldcs_shmem_fdlist[fd].sem_decide, errno,strerror(errno));


  /* server is who graps the semaphore */
  if(!sem_trywait(ldcs_shmem_fdlist[fd].sem_decide))  iamserver=1;
  else iamserver=0;
  debug_printf3("after check sem ldcs_sem_server iamserver = %d\n",iamserver);

  if(iamserver) {
    ldcs_shmem_fdlist[fd].type=LDCS_SHMEM_FD_TYPE_SERVER;
      
  } else {
    ldcs_shmem_fdlist[fd].type=LDCS_SHMEM_FD_TYPE_CONN;
  }

  _ldcs_init_datastruct_shmem(fd);

  free(shmem_basename);

  return(fd);
};


int ldcs_is_server_shmem(int fd) {
  if ((fd<0) || (fd>MAX_FD) )  _error("wrong fd");
  return(ldcs_shmem_fdlist[fd].type==LDCS_SHMEM_FD_TYPE_SERVER);
}

int ldcs_is_client_shmem(int fd) {
  if ((fd<0) || (fd>MAX_FD) )  _error("wrong fd");
  return(ldcs_shmem_fdlist[fd].type==LDCS_SHMEM_FD_TYPE_CONN);
}

/* ************************************************************** */
/* message transfer functions                                     */
/* ************************************************************** */
int ldcs_send_msg_shmem(int fd, ldcs_message_t * msg) {

  if ((fd<0) || (fd>MAX_FD) )  _error("wrong fd");

#if 0
  char help[41];
  int n, connfd;
  connfd=ldcs_shmem_fdlist[fd].fd;

  bzero(help,41);if(msg->data) strncpy(help,msg->data,40);
  debug_printf3("sending message of type: %s len=%d data=%s ...\n",
	       _message_type_to_str(msg->header.type),
	       msg->header.len,help );

  n = _ldcs_write_shmem(connfd,&msg->header,sizeof(msg->header));
  if (n < 0) _error("ERROR writing header to shmem");

  if(msg->header.len>0) {
    n = _ldcs_write_shmem(connfd,(void *) msg->data,msg->header.len);
    if (n < 0) _error("ERROR writing data to shmem");
  }
#endif
  
  return(0);
}

ldcs_message_t * ldcs_recv_msg_shmem(int fd,  ldcs_read_block_t block) {
  ldcs_message_t *msg=NULL;

  if ((fd<0) || (fd>MAX_FD) )  _error("wrong fd");

#if 0
  char help[41];
  int n, connfd;
  connfd=ldcs_shmem_fdlist[fd].fd;

  msg = (ldcs_message_t *) malloc(sizeof(ldcs_message_t));
  if (!msg)  _error("could not allocate memory for message");
  

  n = _ldcs_read_shmem(connfd,&msg->header,sizeof(msg->header), block);
  if (n == 0) {
    free(msg);
    return(NULL);
  }
  if (n < 0) _error("ERROR reading header from shmem");

  if(msg->header.len>0) {

    msg->data = (char *) malloc(msg->header.len);
    if (!msg)  _error("could not allocate memory for message data");
    
    n = _ldcs_read_shmem(connfd,msg->data,msg->header.len, LDCS_READ_BLOCK);
    if (n < 0) _error("ERROR reading message data from shmem");
    if (n != msg->header.len) _error("received different number of bytes for message data");

  } else {
    msg->data = NULL;
  }

  bzero(help,41);if(msg->data) strncpy(help,msg->data,40);
  debug_printf3("received message of type: %s len=%d data=%s ...\n",
	       _message_type_to_str(msg->header.type),
	       msg->header.len,help );

#endif  
  return(msg);
}


int ldcs_recv_msg_static_shmem(int fd, ldcs_message_t *msg,  ldcs_read_block_t block) {
  if ((fd<0) || (fd>MAX_FD) )  _error("wrong fd");

#if 0
  char help[41];
  int rc=0;
  int n, connfd;
  connfd=ldcs_shmem_fdlist[fd].fd;


  n = _ldcs_read_shmem(connfd,&msg->header,sizeof(msg->header), block);
  if (n == 0) return(rc);
  if (n < 0) _error("ERROR reading header from shmem");

  if(msg->header.len>msg->alloclen) {
    _error("ERROR message too long");
  }

  if(msg->header.len>0) {

    msg->data = (char *) malloc(msg->header.len);
    if (!msg)  _error("could not allocate memory for message data");
    
    n = _ldcs_read_shmem(connfd,msg->data,msg->header.len, LDCS_READ_BLOCK);
    if (n == 0) return(rc);
    if (n < 0) _error("ERROR reading message data from shmem");
    if (n != msg->header.len) _error("received different number of bytes for message data");

  } else {
    msg->data = NULL;
  }

  bzero(help,41);if(msg->data) strncpy(help,msg->data,40);
  debug_printf3("received message of type: %s len=%d data=%s ...\n",
	       _message_type_to_str(msg->header.type),
	       msg->header.len,help );
#endif
  
  return(0);
}

int _ldcs_read_shmem(int fd, void *data, int bytes, ldcs_read_block_t block) {

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
          debug_printf3("read from shmem: got EAGAIN or EWOULDBLOCK\n");
          if(block==LDCS_READ_NO_BLOCK) return(0);
          else continue;
       } else { 
          debug_printf3("read from shmem: %ld bytes ... errno=%d (%s)\n",bread,errno,strerror(errno));
       }
    } else {
       debug_printf3("read from shmem: %ld bytes ...\n",bread);
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

int _ldcs_write_shmem(int fd, const void *data, int bytes ) {
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

