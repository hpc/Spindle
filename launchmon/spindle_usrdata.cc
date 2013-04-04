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

#include "spindle_usrdata.h"

int packbefe_cb ( void* udata, 
		  void* msgbuf, 
		  int msgbufmax, 
		  int* msgbuflen ) {
  ldcs_host_port_list_t *host_port_list = (ldcs_host_port_list_t *) udata;
  char *trav= (char *) msgbuf;

  if ( ( msgbuf == NULL ) || ( msgbufmax < 0) ) return -1; 

  int accum_len = host_port_list->size*(HOSTNAME_LEN+sizeof(int)) + sizeof(int);
  
  if ( accum_len > msgbufmax ) return -1; 
  
  memcpy ( (void*) trav, &host_port_list->size, sizeof(int));
  trav += sizeof(int); 

  memcpy ( (void*) trav, host_port_list->hostlist, host_port_list->size*HOSTNAME_LEN); 
  trav += host_port_list->size*HOSTNAME_LEN; 

  memcpy ( (void*) trav, host_port_list->portlist, host_port_list->size*sizeof(int)); 
  trav += host_port_list->size*sizeof(int); 
 
  (*msgbuflen) = accum_len;

   return (0);
}


int unpackfebe_cb  ( void* udatabuf, 
		     int udatabuflen, 
		     void* udata ) {
  
  ldcs_host_port_list_t *host_port_list = (ldcs_host_port_list_t *) udata;
  char *trav= (char *) udatabuf;
  int size;

  if ( ( udatabuf == NULL ) || (udatabuflen < 0) ) return -1; 

  size= * (int *) trav;
  trav += sizeof(int);

  if ( size  < 0 ) return -1; 
  
  host_port_list->size=size;
  host_port_list->hostlist=(char *) malloc(size*HOSTNAME_LEN);
  host_port_list->portlist=(int *) malloc(size*sizeof(int));

  memcpy ( host_port_list->hostlist, (void*) trav, host_port_list->size*HOSTNAME_LEN); 
  trav += host_port_list->size*HOSTNAME_LEN; 

  memcpy ( host_port_list->portlist, (void*) trav, host_port_list->size*sizeof(int)); 
  trav += host_port_list->size*sizeof(int); 
 
  return 0;    
}


int packfebe_cb ( void *udata, 
		  void *msgbuf, 
		  int msgbufmax, 
		  int *msgbuflen ) {
  
  ldcs_host_port_list_t *host_port_list = (ldcs_host_port_list_t *) udata;
  char *trav= (char *) msgbuf;

  if ( ( msgbuf == NULL ) || ( msgbufmax < 0) ) return -1; 

  int accum_len = host_port_list->size*(HOSTNAME_LEN+sizeof(int)) + sizeof(int);
  
  if ( accum_len > msgbufmax ) return -1; 
  
  memcpy ( (void*) trav, &host_port_list->size, sizeof(int));
  trav += sizeof(int); 

  memcpy ( (void*) trav, host_port_list->hostlist, host_port_list->size*HOSTNAME_LEN); 
  trav += host_port_list->size*HOSTNAME_LEN; 

  memcpy ( (void*) trav, host_port_list->portlist, host_port_list->size*sizeof(int)); 
  trav += host_port_list->size*sizeof(int); 
 
  (*msgbuflen) = accum_len;

   return (0);

}

int unpackbefe_cb  ( void* udatabuf, 
		     int udatabuflen, 
		     void* udata ) {

  ldcs_host_port_list_t *host_port_list = (ldcs_host_port_list_t *) udata;
  char *trav= (char *) udatabuf;
  int size;

  if ( ( udatabuf == NULL ) || (udatabuflen < 0) ) return -1; 

  size= * (int *) trav;
  trav += sizeof(int);

  if ( size  < 0 ) return -1; 
  
  host_port_list->size=size;
  host_port_list->hostlist=(char *) malloc(size*HOSTNAME_LEN);
  host_port_list->portlist=(int *) malloc(size*sizeof(int));

  memcpy ( host_port_list->hostlist, (void*) trav, host_port_list->size*HOSTNAME_LEN); 
  trav += host_port_list->size*HOSTNAME_LEN; 

  memcpy ( host_port_list->portlist, (void*) trav, host_port_list->size*sizeof(int)); 
  trav += host_port_list->size*sizeof(int); 
 
  return 0;    
  
}
