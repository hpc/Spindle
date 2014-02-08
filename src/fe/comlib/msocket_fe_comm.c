/*
This file is part of Spindle.  For copyright information see the COPYRIGHT 
file in the top level directory, or at 
https://github.com/hpc/Spindle/blob/master/COPYRIGHT

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

#include "ldcs_cobo.h"
#include "cobo_comm.h"
#include "spindle_debug.h"
#include "ldcs_api.h"
#include "fe_comm.h"
#include "config.h"

struct ldcs_msocket_fe_data_struct
{
  int connid;
};
typedef struct ldcs_msocket_fe_data_struct ldcs_msocket_fe_data_t;

/* FRONT END functions */
int ldcs_audit_server_fe_md_open ( char **hostlist, int hostlistsize, unsigned int port, void **data  ) {
  int rc=0;
  int num_ports   = 20;
  int num_hosts   = -1;
  int* portlist = malloc(num_ports * sizeof(int));
  int i, sersize, connid;
  char *serdata; 
  ldcs_message_t* msg;
  ldcs_message_t *in_msg;
  ldcs_msocket_hostinfo_t hostinfo;  
  int myerank, esize;
  int *eportlist;
  char **ehostlist; 
  ldcs_msocket_fe_data_t *fe_data;
  
  fe_data=(ldcs_msocket_fe_data_t *) malloc(sizeof(ldcs_msocket_fe_data_t));
  if(!fe_data) _error("could not allocate fe_data");


  /* get info about server over external fabric, if available */
  if(ldcs_msocket_external_fabric_CB_registered) {

      char buffer[MAX_PATH_LEN];
      rc=gethostname(buffer, MAX_PATH_LEN);
      if(!strcmp(buffer,"zam371guest")) {
	strcpy(buffer,"localhost");
      }
      ldcs_msocket_external_fabric_CB(buffer, -1, &myerank, &esize, &ehostlist, &eportlist, ldcs_msocket_external_fabric_CB_data);
      num_hosts=esize;
  } else {

    num_hosts=hostlistsize;
    /* build our portlist */
    for (i=0; i<num_ports; i++) {
      portlist[i] = 5000 + i;
    }

  }

  /* connect to first host */
  if(ldcs_msocket_external_fabric_CB_registered) {
    connid=ldcs_audit_server_md_msocket_connect(ehostlist[0], eportlist, 1);
    debug_printf3("after connect connid=%d (E: %s,%d)\n",connid,ehostlist[0], eportlist[0]);
  } else {
    connid=ldcs_audit_server_md_msocket_connect(hostlist[0], portlist, num_ports);
    debug_printf3("after connect connid=%d (D: %s,%d)\n",connid,hostlist[0], portlist[0]);
  }

  /* message for hostinfo */
  debug_printf3("send msg hostinfo\n");
  msg=ldcs_msg_new();

  hostinfo.rank=0;
  hostinfo.size=num_hosts;
  hostinfo.depth=0;  hostinfo.cinfo_from=hostinfo.cinfo_to=-1; /* hostinfo.cinfo_dir=-1; */ 

  msg->header.type=LDCS_MSG_MD_HOSTINFO;
  msg->header.mtype=LDCS_MSG_MTYPE_P2P;
  msg->header.source=-1;  msg->header.from=-1; 
  msg->header.dest=0;   
  msg->header.len=sizeof(hostinfo);
  msg->alloclen=sizeof(hostinfo);
  msg->data=(char *) &hostinfo;

  ldcs_send_msg_socket(connid,msg);

  if(!ldcs_msocket_external_fabric_CB_registered) {

    debug_printf3("processing hostlist of size %d\n",hostlistsize);
    ldcs_audit_server_md_msocket_serialize_hostlist(hostlist, hostlistsize, &serdata, &sersize);
    debug_printf3("serialized hostlist has size %d\n",sersize);
    
    /* message for hostlist */
    msg->header.type=LDCS_MSG_MD_HOSTLIST;
    msg->header.mtype=LDCS_MSG_MTYPE_P2P;
    msg->header.len=sersize;
    msg->alloclen=sersize;
    msg->data=serdata;

    /* sent message to first host */
    debug_printf3("send msg hostlist\n");
    ldcs_send_msg_socket(connid,msg);
  }

  /* message for start of bootstrap */
  msg->header.type=LDCS_MSG_MD_BOOTSTRAP;
  msg->header.mtype=LDCS_MSG_MTYPE_P2P;
  msg->header.len=0;
  msg->alloclen=0;
  msg->data=NULL;

  /* sent message to first host */
  debug_printf3("send msg bootstrap\n");
  ldcs_send_msg_socket(connid,msg);

  /* message for ending bootstrap */
  msg->header.type=LDCS_MSG_MD_BOOTSTRAP_END;
  msg->header.mtype=LDCS_MSG_MTYPE_BCAST;
  msg->header.source=-1;  msg->header.from=-1;   msg->header.dest=-1;   
  msg->header.len=0;
  msg->alloclen=0;
  msg->data=NULL;

  /* sent message to first host */
  debug_printf3("send msg bootstrap end\n");
  ldcs_send_msg_socket(connid,msg);


  /* receive msg from root node whether bootstrap ended  */
  in_msg=ldcs_recv_msg_socket(connid, LDCS_READ_BLOCK);
  if(in_msg->header.type==LDCS_MSG_MD_BOOTSTRAP_END_OK) {
    fprintf(stderr, "SERVERFE: bootstrap  --> successful\n");
  } else {
    fprintf(stderr, "SERVERFE: bootstrap  --> not successful\n");
  }
  ldcs_msg_free(&in_msg);

  fe_data->connid=connid;
  *data=fe_data;

  return(rc);

}

int ldcs_audit_server_fe_md_preload ( char *filename, void *data  ) {
  int rc=0;
  char buffer[MAX_PATH_LEN];
  ldcs_msocket_fe_data_t *fe_data=(ldcs_msocket_fe_data_t *) data;
  FILE * fp;
  char * line = NULL, *p;
  size_t len = 0;
  ssize_t read;
  ldcs_message_t out_msg;
  ldcs_message_t *in_msg;
  
  fprintf(stderr,"SERVERFE: open file for list of library for preload: '%s'\n",filename); 
  fp = fopen(filename, "r");  
  if (fp == NULL)  {
    fprintf(stderr,"SPINDLE FE: preloadfile not found: '%s' ... skipping \n",filename); 
    return(rc);
  }

  out_msg.header.type=LDCS_MSG_PRELOAD_FILE;
  out_msg.alloclen=MAX_PATH_LEN;  out_msg.data=buffer;

  while( (read = getline(&line, &len, fp)) >= 0) {
    
    if( (p=strchr(line,'\n')) ) *p='\0';
    if( (p=strchr(line,' ')) ) *p='\0';
    strcpy(buffer,line);

    fprintf(stderr,"SERVERFE: found library for preload: '%s'\n",line); 
    if( buffer[0]=='#') continue;

    if ( strlen(buffer)==0 ) {
      fprintf(stderr,"SERVERFE: unknown line: '%s'\n",buffer);
      continue;
    }
    out_msg.header.len=strlen(buffer)+1;
    
    debug_printf3("SERVERFE: sent preload msg(%s)  to tree root connid=%d\n",out_msg.data, fe_data->connid);
    ldcs_send_msg_socket(fe_data->connid,&out_msg);
    
    fprintf(stderr, "SERVERFE: sent preload msg(%s)  to tree root\n",out_msg.data);

    /* receive msg from root node */
    in_msg=ldcs_recv_msg_socket(fe_data->connid, LDCS_READ_BLOCK);
    if(in_msg->header.type==LDCS_MSG_PRELOAD_FILE_OK) {
      fprintf(stderr, "SERVERFE:   --> successful\n");
    } else {
      fprintf(stderr, "SERVERFE:   --> not successful\n");
    }
    ldcs_msg_free(&in_msg);
    
  }
  fclose(fp);
  if (line) free(line);

  return(rc);
}

int ldcs_audit_server_fe_md_close ( void *data  ) {
  int rc=0;

#if 0
  int root_fd, ack;

  cobo_server_get_root_socket(&root_fd);
  
  ldcs_cobo_read_fd(root_fd, &ack, sizeof(ack));
  printf("server_rsh_ldcs: got ack=%d from tree root\n",ack);
  
  ack=15;
  ldcs_cobo_write_fd(root_fd, &ack, sizeof(ack));
  printf("server_rsh_ldcs: sent ack=%d to tree root\n",ack);

  /* open and close the server */
  cobo_server_close();
#endif

  return(rc);
}
