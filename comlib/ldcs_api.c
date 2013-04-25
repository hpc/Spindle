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

#include "ldcs_api.h"

/* ************************************************************** */
/* Shortcut functions                                             */
/* ************************************************************** */
/* newpath is pointer to an string stored in internal memory, 
   do not free!  */

int ldcs_send_FILE_QUERY (int fd, char* path, char** newpath) {
  ldcs_message_t message;
  char buffer[MAX_PATH_LEN+1];
  buffer[MAX_PATH_LEN] = '\0';
  
  /* only static allocation for funtions running during rtld_audit */
  ldcs_msg_init(&message);
  message.header.type=LDCS_MSG_FILE_QUERY;
  message.header.len=strlen(path)+1;
  message.data=buffer;
  
  if (message.header.len>MAX_PATH_LEN)  _error("path too long for message");
  strncpy(message.data,path,message.header.len);

  debug_printf3("sending message of type: %s len=%d data='%s' ...(%s)\n",
	       _message_type_to_str(message.header.type),
	       message.header.len,message.data, path );  

  ldcs_send_msg(fd,&message);

  /* get new filename */
  ldcs_recv_msg_static(fd,&message,LDCS_READ_BLOCK);
  if( (message.header.type==LDCS_MSG_FILE_QUERY_ANSWER) && (message.header.len>0) ) {
    *newpath=strdup(message.data);
  } else {
    *newpath=NULL;
    return(-1);
  } 
  return(0);
}

int ldcs_send_FILE_QUERY_EXACT_PATH (int fd, char* path, char** newpath) {
  ldcs_message_t message;
  char buffer[MAX_PATH_LEN+1];
  buffer[MAX_PATH_LEN] = '\0';
  
  /* only static allocation for funtions running during rtld_audit */
  ldcs_msg_init(&message);
  message.header.type=LDCS_MSG_FILE_QUERY_EXACT_PATH;
  message.header.len=strlen(path)+1;
  message.data=buffer;
  
  if (message.header.len>MAX_PATH_LEN)  _error("path too long for message");
  strncpy(message.data,path,message.header.len);

  debug_printf3("sending message of type: %s len=%d data='%s' ...(%s)\n",
	       _message_type_to_str(message.header.type),
	       message.header.len,message.data, path );  

  ldcs_send_msg(fd,&message);

  /* get new filename */
  ldcs_recv_msg_static(fd,&message,LDCS_READ_BLOCK);
  if( (message.header.type==LDCS_MSG_FILE_QUERY_ANSWER) && (message.header.len>0) ) {
    *newpath=strdup(message.data);
  } else {
    *newpath=NULL;
    return(-1);
  } 
  return(0);
}

int ldcs_send_CWD (int fd) {
  ldcs_message_t message;
  char buffer[MAX_PATH_LEN+1];
  int rc=0;
  buffer[MAX_PATH_LEN] = '\0';
  
  ldcs_msg_init(&message);
  message.header.type=LDCS_MSG_CWD;

  if(! getcwd(buffer, MAX_PATH_LEN)) {
    return(-1);
  }
  message.header.len=strlen(buffer)+1;
  message.data=buffer;

  ldcs_send_msg(fd,&message);
  return(rc);
}

int ldcs_send_HOSTNAME (int fd) {
  ldcs_message_t message;
  char buffer[MAX_PATH_LEN+1];
  int rc=0;
  buffer[MAX_PATH_LEN] = '\0';

  ldcs_msg_init(&message);
  message.header.type=LDCS_MSG_HOSTNAME;
  rc=gethostname(buffer, MAX_PATH_LEN);
  debug_printf3("gethostname: rc=%d buffer=%s\n", rc, buffer);  
  
  if(rc!=0) {
    return(-1);
  }
  message.header.len=strlen(buffer)+1;
  message.data=buffer;

  ldcs_send_msg(fd,&message);
  return(rc);
}

int ldcs_send_PID (int fd) {
  ldcs_message_t message;
  int mypid;
  char buffer[16];
  int rc=0;

  ldcs_msg_init(&message);
  message.header.type=LDCS_MSG_PID;

  mypid=getpid();
  sprintf(buffer,"%15d",mypid);
  message.header.len=16;
  message.data=buffer;

  ldcs_send_msg(fd,&message);
  return(rc);
}


int ldcs_send_LOCATION (int fd, char *location) {
  ldcs_message_t message;
  int rc=0;

  ldcs_msg_init(&message);
  message.header.type=LDCS_MSG_LOCATION;
  debug_printf3("location is %s\n", location);  
  
  if(rc!=0) {
    return(-1);
  }
  message.header.len=strlen(location)+1;
  message.data=location;

  ldcs_send_msg(fd,&message);
  return(rc);
}

int ldcs_send_MYRANKINFO_QUERY (int fd, int *mylrank, int *mylsize, int *mymdrank, int *mymdsize) {
  ldcs_message_t message;
  char buffer[MAX_PATH_LEN];
  int *p;
  /* only static allocation for funtions running during rtld_audit */
  ldcs_msg_init(&message);
  message.header.type=LDCS_MSG_MYRANKINFO_QUERY;
  message.header.len=0;
  message.data=buffer;
  
  debug_printf3("sending message of type: %s len=%d\n",
	       _message_type_to_str(message.header.type),message.header.len);  

  ldcs_send_msg(fd,&message);

  /* get new filename */
  ldcs_recv_msg_static(fd,&message,LDCS_READ_BLOCK);
  if( (message.header.type==LDCS_MSG_MYRANKINFO_QUERY_ANSWER) && (message.header.len==(4*sizeof(int)) ) ) {
    p=(int *) message.data;
    *mylrank= *p; p++;
    *mylsize= *p; p++;
    *mymdrank= *p; p++;
    *mymdsize= *p;
    debug_printf3("received rank info: local: %d of %d md: %d of %d\n", *mylrank, *mylsize, *mymdrank, *mymdsize);  
    
  } else {
    *mylrank=*mylsize=*mymdrank=*mymdsize=-1;
    return(-1);
  } 
  return(0);
}


int ldcs_send_END (int fd) {
  ldcs_message_t * message_ptr;
  int rc=0;

  message_ptr = (ldcs_message_t *) malloc(sizeof(ldcs_message_t));
  if (!message_ptr)  _error("could not allocate memory for message");

  message_ptr->header.type=LDCS_MSG_END;
  message_ptr->header.len=0;
  message_ptr->data=NULL;
  ldcs_send_msg(fd,message_ptr);
  ldcs_msg_free(&message_ptr);

  return(rc);
}

