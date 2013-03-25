#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <lmon_api/common.h>
#include <lmon_api/lmon_proctab.h>
#include <lmon_api/lmon_be.h>

#include "ldcs_api.h"
#include "spindle_usrdata.h"
#include "spindle_external_fabric.h"

int spindle_external_fabric_be_CB ( char *myhostname, int myport, int *myrank, int *size, char ***hostlist, int **portlist, 
				    void *data ) {

  spindle_external_fabric_data_t *spindle_external_fabric_data = (spindle_external_fabric_data_t *) data; 
  int rc=0;
  char hostname[HOSTNAME_LEN];
  char** myhostlist;
  int  * myportlist;
  lmon_rc_e lrc;
  int  port,i, hc;
  ldcs_host_port_list_t host_port_list;

  *myrank=spindle_external_fabric_data->md_rank;
  *size=spindle_external_fabric_data->md_size;

  debug_printf3("starting external fabric cb on BE rank %d\n",*myrank);

  if ( LMON_be_amIMaster() == LMON_YES ) {
    host_port_list.size=*size;
    host_port_list.hostlist=(char *) malloc(*size*HOSTNAME_LEN);
    host_port_list.portlist=(int *) malloc(*size*sizeof(int));
  } else {
    host_port_list.size=0;
    host_port_list.hostlist=NULL;
    host_port_list.portlist=NULL;
  }

  strncpy(hostname,myhostname,HOSTNAME_LEN);
  if (( lrc = LMON_be_gather ( hostname, HOSTNAME_LEN, host_port_list.hostlist )) != LMON_OK) {
    fprintf(stdout,     "[LMON BE(%d)] FAILED: LMON_be_gather\n",  *myrank );
    LMON_be_finalize();
    return EXIT_FAILURE;
  }

  port=myport; 
  if (( lrc = LMON_be_gather ( &port, sizeof(int), host_port_list.portlist )) != LMON_OK) {
    fprintf(stdout,     "[LMON BE(%d)] FAILED: LMON_be_gather\n",  *myrank );
    LMON_be_finalize();
    return EXIT_FAILURE;
  }
  
  if ( LMON_be_amIMaster() == LMON_YES ) {

    for(i=0; i < *size; i++)  {
      printf("[LMON BE(%d)] HOSTLIST[%d] %s, %d\n", *myrank, i, &host_port_list.hostlist[i*HOSTNAME_LEN], host_port_list.portlist[i] );
    }
    
    if ( ( lrc = LMON_be_sendUsrData ( (void*) &host_port_list )) != LMON_OK )    {
      fprintf(stdout, "[LMON BE(%d): FAILED] LMON_be_sendUsrData\n", *myrank );
      LMON_be_finalize();
      return EXIT_FAILURE;
    }

  }

  
  myhostlist = (char**) malloc(host_port_list.size * sizeof(char*));
  myportlist = (int  *) malloc(host_port_list.size * sizeof(int));
  
  hc=0;
  for(i=0; i < host_port_list.size; i++)  {
    printf("[LMON FE] HOSTLIST[%d] %s, %d\n", i, &host_port_list.hostlist[i*HOSTNAME_LEN], host_port_list.portlist[i] );
    myhostlist[hc] = strdup(host_port_list.hostlist+i*HOSTNAME_LEN);
    myportlist[hc] = host_port_list.portlist[i];
    debug_printf3("hostname[%d]=%s portlist[%d]=%d\n",hc,myhostlist[hc],hc,myportlist[hc]);
    hc++;
  }

  *hostlist=myhostlist;
  *portlist=myportlist;




  debug_printf3("ending external fabric cb on BE\n");
  return(rc);
}

