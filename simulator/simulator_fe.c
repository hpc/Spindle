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
#include "ldcs_audit_server_md.h"

#include "ldcs_cobo.h"
#include "cobo_comm.h"


#define MSG_SIZE 256

int simulator_fe ( MPI_Comm mycomm, int num_server,
		   char  *location, int locmod, int number, MPI_Comm mycomm_FE_CL ) {
  int rc=0;
  int i;
  char rhostname[100];
  MPI_Status status;
  void *md_data_ptr;


  /* build our hostlist */
  char** hostlist = (char**) malloc(num_server * sizeof(char*));
  for (i = 0; i < num_server; i++) {
    MPI_Recv(rhostname,100,MPI_CHAR,MPI_ANY_SOURCE, 2000+i, MPI_COMM_WORLD, &status);
    if(!strcmp(rhostname,"zam371guest")) {
      hostlist[i] = strdup("localhost");
    } else {
      hostlist[i] = strdup(rhostname);
    }
    debug_printf3("fe got hostname[%d]=%s\n",i,hostlist[i]);
  }
  
  debug_printf3("startup fe (%s, %d)\n",location,number);
  printf("FE: startup fe (%s, %d)\n",location,number);

  /* open the server */
  ldcs_audit_server_fe_md_open(hostlist, num_server, &md_data_ptr);

  debug_printf3("startup fe (%s, %d) ready\n",location,number);
  printf("FE: startup fe (%s, %d) ready\n",location,number);

  /* signal that server are started */
  printf("FE: wait for global barrier\n");
  MPI_Barrier(MPI_COMM_WORLD);

  printf("FE: starting preload (%s, %d) ready\n",location,number);
  ldcs_audit_server_fe_md_preload("./file_preload.dat", md_data_ptr );

  debug_printf3("preload fe (%s, %d) ready\n",location,number);
  printf("FE: preload fe (%s, %d) ready\n",location,number);

  /* signal clients that server are ready */
  MPI_Barrier(mycomm_FE_CL);

  
  ldcs_audit_server_fe_md_close(md_data_ptr);
  
  for (i = 0; i < num_server; i++) free(hostlist[i]);
  free(hostlist);
  debug_printf3("shutdown server (%s, %d)\n",location,number);
  printf("FE: shutdown server (%s, %d)\n",location,number);
  
  return(rc);
}
