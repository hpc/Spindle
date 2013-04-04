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
#include <sys/inotify.h>
#include <errno.h>

#include "ldcs_api.h"
#include "ldcs_api_socket.h"
#include "ldcs_audit_server_md_msocket_topo.h"
#include "ldcs_audit_server_md_msocket_util.h"

int ldcs_audit_server_md_msocket_create_server(char *hostname, int *portlist, int num_ports, int *portused) {
  int fd=-1;
  int port, portindex;

  portindex=0;
  
  while((fd<0) && (portindex<num_ports)) { 
    port=portlist[portindex];
    fd=ldcs_create_server_socket(hostname, port);
    portindex++;
  }
  *portused=port;
  return(fd);
}


int ldcs_audit_server_md_msocket_connect(char *hostname, int *portlist, int num_ports) {
  int fd=-1;
  int port, portindex;

  portindex=0;
  
  while((fd<0) && (portindex<num_ports)) { 
    port=portlist[portindex];
    fd=ldcs_open_connection_socket(hostname, port);
    portindex++;
  }
  
  return(fd);
}


/* from cobo, to support same interface */
int ldcs_audit_server_md_msocket_serialize_hostlist(char **hostlist, int num_hosts, char **rdata, int *rsize) {
  int rc=0;
  int i;
  int size = 0;
  char *data=NULL;
  int datasize=0;


  /* check that we have some hosts in the hostlist */
  if (num_hosts <= 0) {
    return (-1);
  }
  
  /* determine the total number of bytes to hold the strings including terminating NUL character */
  for (i=0; i < num_hosts; i++) {
    size += strlen(hostlist[i]) + 1;
  }
  
  /* determine and allocate the total number of bytes to hold the strings plus offset table */
  datasize = num_hosts * sizeof(int) + size;
  data     = malloc(datasize);
  if (data == NULL) {
    _error("Failed to allocate hostname table");
  }

  /* copy the strings in and fill in the offsets */
  int offset = num_hosts * sizeof(int);
  for (i=0; i < num_hosts; i++) {
    ((int*)data)[i] = offset;
    strcpy((char*)(data + offset), hostlist[i]);
    offset += strlen(hostlist[i]) + 1;
  }

  *rdata=data;
  *rsize=size;

  return(rc);

}

/* Allocates a string containing the hostname for specified rank.
 * The return string must be freed by the caller. */
char* ldcs_audit_server_md_msocket_expand_hostname(char *rdata, int rsize, int rank) {

  if (rdata == NULL) {
    return NULL;
  }

  int* offset = (int*) (rdata + rank * sizeof(int));
  char* hostname = (char*) (rdata + *offset);
  
  return strdup(hostname);
}


int ldcs_audit_server_md_msocket_get_free_connection_table_entry (ldcs_msocket_data_t *ldcs_msocket_data) {
  int nc=-1;
  if (ldcs_msocket_data->connection_table_used >= ldcs_msocket_data->connection_table_size) {
    ldcs_msocket_data->connection_table = realloc(ldcs_msocket_data->connection_table, 
						  (ldcs_msocket_data->connection_table_used + 16) * sizeof(ldcs_connection_t)
						  );
    ldcs_msocket_data->connection_table_size = ldcs_msocket_data->connection_table_used + 16;
    for(nc=ldcs_msocket_data->connection_table_used;(nc<ldcs_msocket_data->connection_table_used + 16);nc++) {
      ldcs_msocket_data->connection_table[nc].state=LDCS_CONNECTION_STATUS_FREE;
    }
  }
  for(nc=0;(nc<ldcs_msocket_data->connection_table_size);nc++) {
    if (ldcs_msocket_data->connection_table[nc].state==LDCS_CONNECTION_STATUS_FREE) break;
  }
  if(nc==ldcs_msocket_data->connection_table_size) _error("internal error with connection table (table full)");

  ldcs_msocket_data->connection_table_used++;
  ldcs_msocket_data->connection_counter++;
  
  return(nc);
}


ldcs_msocket_bootstrap_t *ldcs_audit_server_md_msocket_new_bootstrap(int max_connections) {
  
  ldcs_msocket_bootstrap_t *bootstrap=NULL;

  /* allocate bootstrap data structure */
  bootstrap=(ldcs_msocket_bootstrap_t *) malloc(sizeof(ldcs_msocket_bootstrap_t));
  if(!bootstrap) _error("could not allocate memory for bootstrap");

  bootstrap->size=0;
  bootstrap->allocsize=max_connections;
  bootstrap->fromlist=(int *) malloc(max_connections * sizeof(int));
  if(!bootstrap->fromlist) _error("could not allocate memory for bootstrap->fromlist");
  bootstrap->tolist=(int *) malloc(max_connections * sizeof(int));
  if(!bootstrap->tolist) _error("could not allocate memory for bootstrap->tolist");
  bootstrap->tohostlist=(char **) malloc(max_connections * sizeof(char *));
  if(!bootstrap->tohostlist) _error("could not allocate memory for bootstrap->toportlist");
  bootstrap->toportlist=(int *) malloc(max_connections * sizeof(int));
  if(!bootstrap->toportlist) _error("could not allocate memory for bootstrap->toportlist");

  return(bootstrap);
}

int ldcs_audit_server_md_msocket_free_bootstrap(ldcs_msocket_bootstrap_t *bootstrap) {
  int rc=0;

  free(bootstrap->fromlist);
  free(bootstrap->tolist);
  free(bootstrap->tohostlist);
  free(bootstrap->toportlist);

  
  return(rc);
}

int ldcs_audit_server_md_msocket_dump_bootstrap(ldcs_msocket_bootstrap_t *bootstrap) {
  int rc=0;
  int i;
  for (i=0; i < bootstrap->size; i++) {
    debug_printf3("DUMP bootstrap: %2d -> %2d (%s,%d)\n",
		 bootstrap->fromlist[i],bootstrap->tolist[i],
		 bootstrap->tohostlist[i],bootstrap->toportlist[i]);
  }
  return(rc);
}

int ldcs_audit_server_md_msocket_serialize_bootstrap(ldcs_msocket_bootstrap_t *bootstrap, char **rdata, int *rsize) {
  int rc=0;
  int i;
  int size = 0;
  char *data=NULL;
  int datasize=0;


  ldcs_audit_server_md_msocket_dump_bootstrap(bootstrap);

  /* check that we have some entries */
  if (bootstrap->size < 0) {
    *rdata=NULL;
    *rsize=0;
    return (-1);
  }
  
  /* determine the total number of bytes to hold the strings including terminating NUL character */
  for (i=0; i < bootstrap->size; i++) {
    size += strlen(bootstrap->tohostlist[i]) + 1;
  }
  
  /* determine and allocate the total number of bytes to hold the strings plus offset table */
  datasize = bootstrap->size * sizeof(int) + size;
  
  /* additional info */
  datasize += ( 3 * bootstrap->size + 2 ) * sizeof(int);

  data     = malloc(datasize);
  if (data == NULL) {
    _error("Failed to allocate hostname table");
  }

  int o = 0;
  ((int*)data)[o] = bootstrap->size; o++;
  ((int*)data)[o] = bootstrap->allocsize; o++;
  
  for (i=0; i < bootstrap->size; i++) {
    ((int*)data)[o] = bootstrap->fromlist[i]; o++;
  }
  for (i=0; i < bootstrap->size; i++) {
    ((int*)data)[o] = bootstrap->tolist[i]; o++;
  }
  for (i=0; i < bootstrap->size; i++) {
    ((int*)data)[o] = bootstrap->toportlist[i]; o++;
  }
  
  /* copy the strings in and fill in the offsets */
  int offset = o * sizeof(int) + bootstrap->size * sizeof(int);
  for (i=0; i < bootstrap->size; i++) {
    ((int*)data)[o+i] = offset;
    strcpy((char*)(data + offset), bootstrap->tohostlist[i]);
    offset += strlen(bootstrap->tohostlist[i]) + 1;
  }

  *rdata=data;
  *rsize=datasize;

  return(rc);

}


int ldcs_audit_server_md_msocket_deserialize_bootstrap(char *data, int datasize, ldcs_msocket_bootstrap_t **rbootstrap) {
  int rc=0;
  ldcs_msocket_bootstrap_t *bootstrap;
  int size;
  int i,o;

  /* check that we have some entries */
  if (datasize <= 0) {
    return (-1);
  }
  
  o=0;
  size=((int*)data)[o];  o++;

  debug_printf3("deserialize: size = %d\n",size);
  
  /* check again i we have some entries */
  if (size <= 0) {
    return (-1);
  }

  bootstrap=ldcs_audit_server_md_msocket_new_bootstrap(size);

  /* allocate bootstrap data structure */
  bootstrap->size=size;
  bootstrap->allocsize=((int*)data)[o]; o++;

  for (i=0; i < bootstrap->size; i++) {
    bootstrap->fromlist[i]=((int*)data)[o]; o++;
    debug_printf3("deserialize: from[%d] = %d\n",i,bootstrap->fromlist[i]);
  }

  for (i=0; i < bootstrap->size; i++) {
    bootstrap->tolist[i]=((int*)data)[o]; o++;
    debug_printf3("deserialize: to[%d] = %d\n",i,bootstrap->tolist[i]);
  }

  for (i=0; i < bootstrap->size; i++) {
    bootstrap->toportlist[i]=((int*)data)[o]; o++;
    debug_printf3("deserialize: toport[%d] = %d\n",i,bootstrap->toportlist[i]);
  }

  /* copy the strings in and fill in the offsets */
  int offset = o * sizeof(int) + bootstrap->size * sizeof(int);
  for (i=0; i < bootstrap->size; i++) {
    offset=((int*)data)[o+i];
    bootstrap->tohostlist[i]=strdup((char*)(data + offset));
    debug_printf3("deserialize: tohost[%d] = %s\n",i,bootstrap->tohostlist[i]);
  }

  ldcs_audit_server_md_msocket_dump_bootstrap(bootstrap);

  *rbootstrap=bootstrap;

  return(rc);

}
