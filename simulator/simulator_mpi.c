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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 

#include <mpi.h>

#include "ldcs_api.h" 

#include "simulator_fe.h" 
#include "simulator_server.h" 
#include "simulator_client.h" 
#include "ldcs_audit_server_md.h"

typedef enum {
   LDCS_SIMULATOR_FE,
   LDCS_SIMULATOR_AUDITSERVER,
   LDCS_SIMULATOR_AUDITCLIENT,
   LDCS_SIMULATOR_UNKNOWN
} simulator_task_t;

char* _tasks_type_to_str (simulator_task_t type) {

  return(
	 (type == LDCS_SIMULATOR_FE)         ? "LDCS_SIMULATOR_FRONTEND___" :
	 (type == LDCS_SIMULATOR_AUDITSERVER)? "LDCS_SIMULATOR_AUDITSERVER" :
	 (type == LDCS_SIMULATOR_AUDITCLIENT)? "LDCS_SIMULATOR_AUDITCLIENT" :
	 (type == LDCS_SIMULATOR_UNKNOWN)    ? "LDCS_SIMULATOR_UNKNOWN" :
	 "???");
}

void error(const char *msg)
{
    perror(msg);
    exit(0);
}

int simulator_external_fabric_CB ( char *myhostname, int myport, int *myrank, int *size, char ***hostlist, int **portlist, 
				   void *data );

int main(int argc, char *argv[]) {
  int rank,size;
  int lrank,lsize;
  int lrank_FE_AS,lsize_FE_AS, color_FE_AS;
  char hostname[100];
  char ldcs_location[MAX_PATH_LEN];
  int ldcs_number, ldcs_locmod, ldcs_tpn, server_num;
  int mytask=LDCS_SIMULATOR_UNKNOWN;
  int num_server;
  MPI_Comm mycomm;
  MPI_Comm mycomm_FE_AS;
  MPI_Comm mycomm_FE_CL;

  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD,&rank);
  MPI_Comm_size(MPI_COMM_WORLD,&size);

  bzero(ldcs_location,MAX_PATH_LEN);

  bzero(hostname,100);
  gethostname(hostname,100);

  if(rank==0) {
    FILE * fp;
    char * line = NULL;
    size_t len = 0;
    ssize_t read;
    
    fp = fopen("simulator.dat", "r");   if (fp == NULL)  perror("could not open simulator.dat");

    if( (read = getline(&line, &len, fp)) == -1) perror("could read line 1 from simulator.dat");
    sscanf(line, "LDCS_LOCATION=%s",ldcs_location);
    printf("SIMULATOR: location=%s\n", ldcs_location);

    if( (read = getline(&line, &len, fp)) == -1) perror("could read line 2 from simulator.dat");
    sscanf(line, "LDCS_NUMBER=%d",&ldcs_number);
    printf("SIMULATOR: number=%d\n", ldcs_number);

    if( (read = getline(&line, &len, fp)) == -1) perror("could read line 3 from simulator.dat");
    sscanf(line, "LDCS_LOCATION_MOD=%d",&ldcs_locmod);
    printf("SIMULATOR: locmod=%d\n", ldcs_locmod);

    if( (read = getline(&line, &len, fp)) == -1) perror("could read line 4 from simulator.dat");
    sscanf(line, "LDCS_TPN=%d",&ldcs_tpn);
    printf("SIMULATOR: tpn=%d\n", ldcs_tpn);

    fclose(fp);
    if (line) free(line);
  }

  MPI_Bcast(ldcs_location,100, MPI_CHAR, 0, MPI_COMM_WORLD);
  MPI_Bcast(&ldcs_number,1, MPI_INT, 0, MPI_COMM_WORLD);
  MPI_Bcast(&ldcs_locmod,1, MPI_INT, 0, MPI_COMM_WORLD);
  MPI_Bcast(&ldcs_tpn,1, MPI_INT, 0, MPI_COMM_WORLD);
  
  /* last task --> FE */
  if(rank==(size-1) ) {
    mytask=LDCS_SIMULATOR_FE;
    color_FE_AS=0;
  } else {
    /* first task of node --> Auditserver 
       all other tasks    --> Auditclient */
    if((rank%ldcs_tpn)==0) {
      mytask=LDCS_SIMULATOR_AUDITSERVER;
      color_FE_AS=rank+1;
    } else {
      mytask=LDCS_SIMULATOR_AUDITCLIENT;
      color_FE_AS=-1;
    }
  } 
  num_server=size/ldcs_tpn;

  MPI_Comm_split(MPI_COMM_WORLD,mytask,rank,&mycomm);
  MPI_Comm_rank(mycomm,&lrank);
  MPI_Comm_size(mycomm,&lsize);

  lrank_FE_AS=lsize_FE_AS=-1;
  MPI_Comm_split(MPI_COMM_WORLD,((mytask==LDCS_SIMULATOR_FE) || (mytask==LDCS_SIMULATOR_AUDITSERVER)) ,color_FE_AS,&mycomm_FE_AS);
  MPI_Comm_rank(mycomm_FE_AS,&lrank_FE_AS);
  MPI_Comm_size(mycomm_FE_AS,&lsize_FE_AS);

  MPI_Comm_split(MPI_COMM_WORLD,((mytask==LDCS_SIMULATOR_FE) || (mytask==LDCS_SIMULATOR_AUDITCLIENT)), rank, &mycomm_FE_CL);
  
  printf("SIMULATOR: %2d of %2d host=%8s task=%-30s start PID=%8d location=%s l=%d of %d l_FE_AS=%d of %d\n",
	 rank,size,hostname,_tasks_type_to_str(mytask), getpid(), ldcs_location, lrank, lsize, lrank_FE_AS, lsize_FE_AS);

  /* set SION debug file name */
  if(1){
    char helpstr[MAX_PATH_LEN];
    sprintf(helpstr,"_debug_sim_%s_l_%02d_of_%02d_t%02d_%s",_tasks_type_to_str(mytask),lrank+1,lsize,rank,hostname);
    setenv("SION_DEBUG",helpstr,1);
    printf("SIMULATOR: set SION_DEBUG=%s\n",helpstr);
  }
  
  if ( (mytask==LDCS_SIMULATOR_FE)  ||
       (mytask==LDCS_SIMULATOR_AUDITSERVER) ) {
    ldcs_register_external_fabric_CB( &simulator_external_fabric_CB, (void *) &mycomm_FE_AS);
  }
 
  /* startup different groups */
  if(mytask==LDCS_SIMULATOR_FE) {

    printf("SIMULATOR: start FE on %d: num_server=%d\n",rank, num_server);
    simulator_fe( mycomm, num_server, ldcs_location, ldcs_locmod, ldcs_number, mycomm_FE_CL);
  }
  if(mytask==LDCS_SIMULATOR_AUDITSERVER) {
    server_num=(int) rank/ldcs_tpn;

    /* send hostname to FE */
    MPI_Send(hostname,100,MPI_CHAR,size-1,2000+server_num,MPI_COMM_WORLD);
    
    printf("SIMULATOR: start server #%d on %d\n",server_num, rank);
    simulator_server( mycomm, ldcs_location, ldcs_locmod, ldcs_number);
  }
  if(mytask==LDCS_SIMULATOR_AUDITCLIENT) {

    /* will be signaled from FE and server after initialization */
    printf("SIMULATOR: wait on Barrier before start client on %d\n",rank);
    MPI_Barrier(MPI_COMM_WORLD);

    printf("SIMULATOR: wait on Barrier from FE before start client on %d\n",rank);
    MPI_Barrier(mycomm_FE_CL);

    printf("SIMULATOR: start client on %d\n",rank);
    simulator_client ( mycomm, ldcs_location, ldcs_locmod, ldcs_number);
  }
  
  

  /* handshake and end of simulation  */
  MPI_Barrier(MPI_COMM_WORLD);
  printf("SIMULATOR: a sleep(3s) on %d\n",rank);
  sleep(3); 
  MPI_Barrier(MPI_COMM_WORLD);

  printf("SIMULATOR: %d of %d finish, ... sleep(3s) after MPI_Finalize ...  \n",rank,size);
  MPI_Finalize();

  /* TDB: check why needed */
  sleep(3); 
  printf("SIMULATOR: %d of %d finished,\n",rank,size);

  return 0;
}


int simulator_external_fabric_CB ( char *myhostname, int myport, int *myrank, int *size, char ***hostlist, int **portlist, 
				   void *data ) {
  int rc=0;
  int lrank, lsize;
  int num_server;
  MPI_Comm *mycomm = (MPI_Comm *) data;

  MPI_Comm_rank(*mycomm,&lrank);
  MPI_Comm_size(*mycomm,&lsize);
  num_server=lsize-1;

  debug_printf3("starting external fabric CB %d of %d (%s,%d)\n",lrank,lsize,myhostname,myport);

  /* build hostlist on root server node*/
  if(lrank==1) {
    char** myhostlist = (char**) malloc(num_server * sizeof(char*));
    int  * myportlist = (int  *) malloc(num_server * sizeof(int));
    char   rhostname[MAX_PATH_LEN];
    int    rport, hc, i;
    MPI_Status status;

    hc=0;
    myhostlist[hc]=strdup(myhostname);
    debug_printf3("hostname[%d]=%s\n",hc,myhostlist[hc]);
    myportlist[hc]=myport;
    debug_printf3("portlist[%d]=%d\n",hc,myportlist[hc]);
    
    hc++;
    for (i = 1; i < num_server; i++) {
      MPI_Recv(rhostname,MAX_PATH_LEN,MPI_CHAR, 1+i, 2000, *mycomm, &status);
      myhostlist[hc] = strdup(rhostname);

      MPI_Recv(&rport,1,MPI_INT, 1+i, 2001, *mycomm, &status);
      myportlist[hc]=rport;
      debug_printf3("hostname[%d]=%s portlist[%d]=%d\n",hc,myhostlist[hc],hc,myportlist[hc]);
      hc++;

    }
    *hostlist=myhostlist;
    *portlist=myportlist;
    
    /* send hostname/port to FE */
    MPI_Send(myhostname,MAX_PATH_LEN,MPI_CHAR,0,3000,*mycomm);
    MPI_Send(&myport,1,MPI_INT,0,3001,*mycomm);
  }

  /* FE */
  if(lrank==0) {
    char** myhostlist = (char**) malloc(1 * sizeof(char*));
    int  * myportlist = (int  *) malloc(1 * sizeof(int));
    char   rhostname[MAX_PATH_LEN];
    int    rport;
    MPI_Status status;

    MPI_Recv(rhostname,MAX_PATH_LEN,MPI_CHAR, 1, 3000, *mycomm, &status);
    myhostlist[0] = strdup(rhostname);
    
    MPI_Recv(&rport,1,MPI_INT, 1, 3001, *mycomm, &status);
    myportlist[0]=rport;

    *hostlist=myhostlist;
    *portlist=myportlist;

    debug_printf3("got rhostname[0]=%s rport[0]=%d\n",myhostlist[0],myportlist[0]);
  }

  /* non root server */
  if(lrank>1) {
    /* send hostname/port to FE */
    MPI_Send(myhostname,MAX_PATH_LEN,MPI_CHAR,1,2000,*mycomm);
    MPI_Send(&myport,1,MPI_INT,1,2001,*mycomm);
    *hostlist=NULL;
    *portlist=NULL;
  }

  *myrank=lrank-1;
  *size=num_server;
  debug_printf3(" return:  myrank=%d size=%d\n",*myrank,*size);

  debug_printf3("leaving external fabric CB %d of %d\n",lrank,lsize);

  return(rc);
}
