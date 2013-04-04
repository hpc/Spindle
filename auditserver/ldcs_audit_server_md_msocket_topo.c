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
#include "ldcs_api_listen.h"
#include "ldcs_audit_server_md_msocket.h"
#include "ldcs_audit_server_md_msocket_util.h"
#include "ldcs_audit_server_md_msocket_topo.h"

int compute_binom_tree(int size, int **connlist, int *connlistsize, int *max_connections);

int ldcs_audit_server_md_msocket_init_topo_bootstrap(ldcs_msocket_data_t *ldcs_msocket_data) {
  int rc=0;
  int rank, c, dest;
  int *connlist,connlistsize, max_connections;
  int sersize;
  char *serdata; 
  int *reachable;
  ldcs_msocket_hostinfo_t *hostinfo=&ldcs_msocket_data->hostinfo;
  ldcs_msocket_bootstrap_t *bootstrap;

  /* allocate init reachable list */
  reachable=(int *) malloc(hostinfo->size * sizeof(int));
  if(!reachable) _error("could noy allocate memory for reachable list");
  reachable[0]=1; for(rank=1;rank<hostinfo->size;rank++) reachable[rank]=0;

  compute_binom_tree(hostinfo->size,  &connlist, &connlistsize, &max_connections);

  for(c=0;c<connlistsize;c++) {
    debug_printf3("connection list: %2d -> %2d \n",connlist[2*c+0],connlist[2*c+1]);
  }

  /* allocate bootstrap data structure */
  bootstrap=ldcs_audit_server_md_msocket_new_bootstrap(max_connections);

  for(rank=0;rank<hostinfo->size;rank++) {
    /* search connlist for rank and build bootstrap data
       structure,incl. hostname info */
    bootstrap->size=0;
    for(c=0;c<connlistsize;c++) {
      if(connlist[2*c+0]==rank) {
	bootstrap->fromlist[bootstrap->size]=connlist[2*c+0];
	dest=connlist[2*c+1];
	bootstrap->tolist[bootstrap->size]=dest;
	bootstrap->tohostlist[bootstrap->size]=ldcs_msocket_data->hostlist[dest]; /* no copy of char !!! */
	bootstrap->toportlist[bootstrap->size]=ldcs_msocket_data->portlist[dest]; 
	bootstrap->size++;
      }
    }

    if(rank==0) {
      /* apply bootstrap (connect to childs, send hostinfo) */
      _ldcs_audit_server_md_msocket_run_topo_bootstrap(ldcs_msocket_data, bootstrap);
    } else {

      if(bootstrap->size>0) {
	ldcs_message_t* msg;
	msg=ldcs_msg_new();
	
	/* serialize  bootstrap info */
	ldcs_audit_server_md_msocket_serialize_bootstrap(bootstrap, &serdata, &sersize);
	
	/* send bootstrap msg to client  */
	msg->header.type=LDCS_MSG_MD_BOOTSTRAP;  msg->header.mtype=LDCS_MSG_MTYPE_P2P;
	msg->header.dest=rank;
	msg->header.source=ldcs_msocket_data->hostinfo.rank;  msg->header.from=ldcs_msocket_data->hostinfo.rank; 
	msg->header.len=sersize;      msg->alloclen=sersize;
	msg->data=serdata;
	
	/* sent message to first host */
	debug_printf3("rank=%d route bootstrap msg %d->%d\n",rank,msg->header.source, msg->header.dest);

	ldcs_audit_server_md_msocket_route_msg(ldcs_msocket_data, msg);

	free(serdata);msg->data=NULL;
	ldcs_msg_free(&msg);
      }
    }

  }

  free(reachable);
  ldcs_audit_server_md_msocket_free_bootstrap(bootstrap);
  return(rc);
}

int ldcs_audit_server_md_msocket_run_topo_bootstrap(ldcs_msocket_data_t *ldcs_msocket_data, char *serbootinfo, int serbootlen) {
  int rc=0;
  ldcs_msocket_bootstrap_t *bootstrap; 

  rc=ldcs_audit_server_md_msocket_deserialize_bootstrap(serbootinfo, serbootlen, &bootstrap);
  if(!rc) {
    _ldcs_audit_server_md_msocket_run_topo_bootstrap(ldcs_msocket_data, bootstrap);
    ldcs_audit_server_md_msocket_free_bootstrap(bootstrap);
  }
  return(rc);
}

int _ldcs_audit_server_md_msocket_run_topo_bootstrap(ldcs_msocket_data_t *ldcs_msocket_data, ldcs_msocket_bootstrap_t *bootstrap) {
  int rc=0;
  int c, connid, nc, fd;
  ldcs_message_t* msg;
  ldcs_msocket_hostinfo_t hostinfo;  

  debug_printf3("starting run bootstrab ..\n");
  ldcs_audit_server_md_msocket_dump_bootstrap(bootstrap);

  for(c=0;c<bootstrap->size;c++) {

    if(bootstrap->toportlist[c]>=0) {
      connid=ldcs_audit_server_md_msocket_connect(bootstrap->tohostlist[c],&bootstrap->toportlist[c], 1);
    } else {
      connid=ldcs_audit_server_md_msocket_connect(bootstrap->tohostlist[c],
						  ldcs_msocket_data->default_portlist, ldcs_msocket_data->default_num_ports);
    }
    if(connid<=0) {
      _error("problems connecting child ...");
    }
    /* store connection */
    nc=ldcs_audit_server_md_msocket_get_free_connection_table_entry (ldcs_msocket_data);
    ldcs_msocket_data->connection_table[nc].connid       = connid;
    ldcs_msocket_data->connection_table[nc].state        = LDCS_CONNECTION_STATUS_ACTIVE;
    ldcs_msocket_data->connection_table[nc].null_msg_cnt = 0;    
    ldcs_msocket_data->connection_table[nc].remote_rank  = bootstrap->tolist[c];
    printf("SERVER[%02d]: open connection on host %s #nc=%02d rrank=%d at %12.4f\n", ldcs_msocket_data->md_rank, 
	   ldcs_msocket_data->hostname, 
	   nc, ldcs_msocket_data->connection_table[nc].remote_rank,ldcs_get_time());

    debug_printf3("SERVER[%02d]: open connection on host %s #c=%02d rrank=%d at %12.4f\n", ldcs_msocket_data->md_rank, 
		 ldcs_msocket_data->hostname, ldcs_msocket_data->connection_counter, 
		 ldcs_msocket_data->connection_table[nc].remote_rank,ldcs_get_time());
    
    /* register connection fd to listener */
    fd=ldcs_get_fd_socket(ldcs_msocket_data->connection_table[nc].connid);
    ldcs_listen_register_fd(fd, nc, &_ldcs_audit_server_md_msocket_connection_CB, (void *) ldcs_msocket_data);
    ldcs_msocket_data->server_stat.num_connections++;
    
    /* send hostinfo */
      /* message for hostinfo */
    debug_printf3("send msg hostinfo\n");
    msg=ldcs_msg_new();
    
    hostinfo.rank=bootstrap->tolist[c];    hostinfo.size=ldcs_msocket_data->hostinfo.size;
    hostinfo.depth=0;                      
    hostinfo.cinfo_from=bootstrap->fromlist[c]; hostinfo.cinfo_to=bootstrap->tolist[c];

    msg->header.type=LDCS_MSG_MD_HOSTINFO;  msg->header.mtype=LDCS_MSG_MTYPE_P2P;
    msg->header.dest=bootstrap->tolist[c];
    msg->header.source=ldcs_msocket_data->hostinfo.rank;  msg->header.from=ldcs_msocket_data->hostinfo.rank; 
    msg->header.len=sizeof(hostinfo); msg->alloclen=sizeof(hostinfo);   msg->data=(char *) &hostinfo;
    ldcs_send_msg_socket(connid,msg);
    msg->data=NULL;ldcs_msg_free(&msg);

  }

  return(rc);
}


int ldcs_audit_server_md_msocket_route_msg(ldcs_msocket_data_t *ldcs_msocket_data, ldcs_message_t *msg) {
  return(ldcs_audit_server_md_msocket_route_msg_binom_tree(ldcs_msocket_data, msg));
}

int compute_connections(int size, int **connlist, int *connlistsize, int *max_connections) {
  return(compute_binom_tree(size, connlist, connlistsize, max_connections));
}

int ldcs_audit_server_md_msocket_route_msg_binom_tree(ldcs_msocket_data_t *ldcs_msocket_data, ldcs_message_t *msg) {
  int rrank, nc, found, foundnc, maxrank;
  int rc=0;
  int dest=msg->header.dest;

  debug_printf3("start route on msg %s %d -> %d -> %d\n",_message_type_to_str(msg->header.type),msg->header.source, msg->header.from, msg->header.dest);
  
  if(msg->header.mtype==LDCS_MSG_MTYPE_P2P) {
    found=0; foundnc=-1;maxrank=-1;
    /* searching for max remote rank which less than dest */
    for(nc=0;(nc<ldcs_msocket_data->connection_table_used) && (!found);nc++) {
      rrank=ldcs_msocket_data->connection_table[nc].remote_rank;
      debug_printf3("  check for msg with dest %d child nc=%d with rrank=%d maxrank=%d\n",dest,nc,rrank,maxrank);
      if(rrank<0) continue; /* not to front end */
      if(rrank==dest) {
	found=1;foundnc=nc;maxrank=rrank;
      }
      if(rrank<dest) {
	if(rrank>maxrank) {
	  foundnc=nc;maxrank=rrank;
	}
      }
    }
    if(found || (foundnc>=0) ) {
      debug_printf3("SEND msg with dest %d child nc=%d with rrank=%d connid=%d\n",dest,foundnc,maxrank,ldcs_msocket_data->connection_table[foundnc].connid);
      ldcs_send_msg_socket(ldcs_msocket_data->connection_table[foundnc].connid,msg);
    }
  }

  if(msg->header.mtype==LDCS_MSG_MTYPE_BCAST) {
    /* searching for max remote rank which less than dest */
    msg->header.from=ldcs_msocket_data->hostinfo.rank;
    for(nc=0;(nc<ldcs_msocket_data->connection_table_used);nc++) {
      rrank=ldcs_msocket_data->connection_table[nc].remote_rank;
      debug_printf3("check for bcast msg with dest %d child nc=%d with rrank=%d iam=%d\n",dest,nc,rrank,ldcs_msocket_data->hostinfo.rank);
      if(rrank<0) continue; 	/* not to front end */
      if(rrank<=ldcs_msocket_data->hostinfo.rank) continue; /* not upward in tree */

      debug_printf3("BCAST msg with dest %d to child nc=%d with rrank=%d connid=%d\n",dest,nc,rrank,ldcs_msocket_data->connection_table[nc].connid);
      ldcs_send_msg_socket(ldcs_msocket_data->connection_table[nc].connid,msg);
    }
  }

  debug_printf3("end route on msg %s %d -> %d -> %d\n",_message_type_to_str(msg->header.type),msg->header.source, msg->header.from, msg->header.dest);

  return(rc);
}


  int compute_binom_tree(int size, int **connlist, int *connlistsize, int *max_connections) {
    int rc=0;
    int rank, n=1, ci=0, num_child;
    int *clist, *childs;
    int max_children = 0;

    while (n < size) {
      n <<= 1;
      max_children++;
    }

    clist   = malloc(2*size*sizeof(int));
    if(!clist) _error("could not allocate memory for clist");
    childs  = (int*) malloc(max_children * sizeof(int));
    if(!childs) _error("could not allocate memory for childs");


    for(rank=0;rank<size;rank++) {
      num_child = 0;
    
      /* find our parent rank and the ranks of our children */
      int low  = 0;
      int high = size - 1;
      while (high - low > 0) {
	int mid = (high - low) / 2 + (high - low) % 2 + low;
	if (low == rank) {
	  childs[num_child] = mid;
	  num_child++;
	}
	/* if (mid == rank) { parent = low; } */
	if (mid <= rank) { low  = mid; }
	else             { high = mid-1; }
      }
      for(n=0;n<num_child;n++) {
	clist[ci]=rank;ci++;
	clist[ci]=childs[n];ci++;
      }
    }

    *connlistsize=ci/2;
    *connlist=clist;
    *max_connections=max_children;

    return rc;
  }
