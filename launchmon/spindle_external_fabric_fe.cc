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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <lmon_api/lmon_fe.h>

#include "ldcs_api.h"
#include "spindle_usrdata.h"
#include "spindle_external_fabric.h"

int spindle_external_fabric_fe_CB ( char *myhostname, int myport, int *myrank, int *size, char ***hostlist, int **portlist, 
				    void *data ) {

  int rc=0;

  spindle_external_fabric_data_t *spindle_external_fabric_data = (spindle_external_fabric_data_t *) data;
  lmon_rc_e lrc;
  char** myhostlist;
  int  * myportlist;
  ldcs_host_port_list_t host_port_list;
  int hc,i;

  debug_printf3("starting external fabric cb on FE\n");

  if ( ( lrc = LMON_fe_recvUsrDataBe ( spindle_external_fabric_data->asession, (void*) &host_port_list )) != LMON_OK )    {
    fprintf(stdout, "[LMON FE: FAILED] LMON_be_sendUsrData\n" );
    return EXIT_FAILURE;
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

  *myrank=-1;
  *size=hc;
  *hostlist=myhostlist;
  *portlist=myportlist;

  debug_printf3("ending external fabric cb on FE\n");
  return(rc);

}


