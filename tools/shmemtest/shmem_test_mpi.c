#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h> 

#include <errno.h>

#include <ldcs_api.h>
#include <mpi.h>


#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#define	FILE_MODE	(S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)
#include <fcntl.h>           /* For O_* constants */
#include <sys/stat.h>        /* For mode constants */
#include <semaphore.h>

#define MAX_MSG_SIZE 256
#define MAX_NUM_CLIENTS 64

struct shm_data_struct {
  int    c_clients;
  sem_t *c_in_sem[MAX_NUM_CLIENTS];
  sem_t *c_out_sem[MAX_NUM_CLIENTS];
  int   conn_state[MAX_NUM_CLIENTS];  
  int   in_msg_size[MAX_NUM_CLIENTS];  
  int   out_msg_size[MAX_NUM_CLIENTS];  
  char  in_buffer[MAX_NUM_CLIENTS*MAX_MSG_SIZE]; 
  char  out_buffer[MAX_NUM_CLIENTS*MAX_MSG_SIZE]; 
};
typedef struct shm_data_struct shm_data_t;
  

void error(const char *msg)
{
    perror(msg);
    exit(0);
}

int main(int argc, char *argv[])
{
  int ldcsid, n;
  
  char buffer[256];
  char *result;
  int rank,size;
  char hostname[100];
  char shmemfile[MAX_PATH_LEN];

  char semname[MAX_PATH_LEN];
  sem_t *ldcs_sem_server;
  
  char semshmemlockname[MAX_PATH_LEN];
  sem_t *ldcs_sem_lock;

  char semshmeminitname[MAX_PATH_LEN];
  sem_t *ldcs_sem_init;

  char semshmeminitreadyname[MAX_PATH_LEN];
  sem_t *ldcs_sem_init_ready;

  int fd, iamserver=-1;
  int myclientnr=0;
  
  shm_data_t *shm_data;

  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD,&rank);
  MPI_Comm_size(MPI_COMM_WORLD,&size);

  char* ldcs_location=getenv("LDCS_LOCATION");
  int   ldcs_number  =atoi(getenv("LDCS_NUMBER"));
  char* ldcs_locmodstr=getenv("LDCS_LOCATION_MOD");

  gethostname(hostname,100);

  sprintf(semname,"ldcs_server");
  sprintf(semshmemlockname,"ldcs_shmem_lock");
  sprintf(semshmeminitname,"ldcs_shmem_init");
  sprintf(shmemfile,"ldcs_interchange.dat");

  /* init semaphore for DECIDING WHO IS SERVER */ 
  ldcs_sem_server=sem_open(semname, O_CREAT, FILE_MODE, 1);
  debug_printf("after sem_open semname=%s %x errno(%d,%s)\n",semname, ldcs_sem_server,errno,strerror(errno));
  /* server is who graps the semaphore */
  if(!sem_trywait(ldcs_sem_server))  iamserver=1;
  else iamserver=0;
  debug_printf("after check sem ldcs_sem_server iamserver = %d\n",iamserver);

  /* INIT SHMEM DATA STRUCTURE */ 
  
  /* get lock for data structure */
  ldcs_sem_lock=sem_open(semshmemlockname, O_CREAT, FILE_MODE, 1);
  debug_printf("after open sem ldcs_sem_lock %s %x errno(%d,%s)\n",semshmemlockname,ldcs_sem_lock,errno,strerror(errno));
  sem_wait(ldcs_sem_lock);
  debug_printf("after sem_wait(ldcs_sem_lock)\n");

  /* check if data structure is initialized */
  /* -->  get lock for init, init will not neccesary done by server  */
  ldcs_sem_init=sem_open(semshmeminitname, O_CREAT, FILE_MODE, 1);
  debug_printf("after open sem ldcs_sem_init %s %x errno(%d,%s)\n",semshmeminitname,ldcs_sem_lock,errno,strerror(errno));
  if(!sem_trywait(ldcs_sem_init)) {
    debug_printf("open shmem file = %s + create\n",shmemfile);
    fd=shm_open(shmemfile, O_RDWR|O_CREAT, 0600);
    debug_printf("after open fd=%d\n",fd);

    ftruncate(fd, sizeof(shm_data_t));
    debug_printf("after ftruncate to %d bytes\n",sizeof(shm_data_t));

    shm_data=mmap(NULL, sizeof(shm_data_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    debug_printf("after mmap: %x\n",shm_data);
    
    /* init data structure */
    shm_data->c_clients=0;
    sem_post(ldcs_sem_lock);

  } else {
    debug_printf("open shmem file = %s\n",shmemfile);
    fd=shm_open(shmemfile, O_RDWR, 0600);
    debug_printf("after open fd=%d\n",fd);

    shm_data=mmap(NULL, sizeof(shm_data_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    debug_printf("after mmap: %x\n",shm_data);
    
    /* register as client */
    myclientnr=shm_data->c_clients;
    shm_data->c_clients++;
    
    sem_post(ldcs_sem_lock);
    
  }

  
  sem_wait(ldcs_sem_lock);
  
  debug_printf("open shmem file = %s\n",shmemfile);
  fd=shm_open(shmemfile, O_RDWR, 0600);
  debug_printf("after open fd=%d\n",fd);

  printf("Hello World: Number of clients: %d of %d\n",myclientnr,shm_data->c_clients);

  
  sem_post(ldcs_sem_lock);

  printf("Hello World: %d of %d start PID=%d HOSTNAME=%s %s\n",rank,size,getpid(),hostname,(iamserver)?"SERVER":"CLIENT");


  /* open shmem */


  MPI_Barrier(MPI_COMM_WORLD);

  debug_printf("close shmem file fd = %d\n",fd);
  close(fd);
  debug_printf("after close fd=%d\n",fd);


  if(iamserver) {
    sem_unlink(semname);
  }

  printf("Hello World: %d of %d finished\n",rank,size);

  MPI_Finalize();

  return 0;
}


