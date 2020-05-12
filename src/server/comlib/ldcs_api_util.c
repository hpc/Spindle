/*
This file is part of Spindle.  For copyright information see the COPYRIGHT 
file in the top level directory, or at 
https://github.com/hpc/Spindle/blob/master/COPYRIGHT

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License (as published by the Free Software
Foundation) version 2.1 dated February 1999.  This program is distributed in the
hope that it will be useful, but WITHOUT ANY WARRANTY; without even the IMPLIED
WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms 
and conditions of the GNU Lesser General Public License for more details.  You should 
have received a copy of the GNU Lesser General Public License along with this 
program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "ldcs_api.h"

/* ************************************************************** */
/* Utility functions                                              */
/* ************************************************************** */
#define STR_CASE(X) case X: return # X
char* _message_type_to_str (ldcs_message_ids_t type) {
   switch (type) {
      STR_CASE(LDCS_MSG_FILE_QUERY);
      STR_CASE(LDCS_MSG_FILE_QUERY_ANSWER);
      STR_CASE(LDCS_MSG_FILE_QUERY_EXACT_PATH);
      STR_CASE(LDCS_MSG_FILE_REQUEST);
      STR_CASE(LDCS_MSG_FILE_ERRCODE);
      STR_CASE(LDCS_MSG_STAT_QUERY);
      STR_CASE(LDCS_MSG_STAT_ANSWER);
      STR_CASE(LDCS_MSG_STAT_NET_REQUEST);
      STR_CASE(LDCS_MSG_STAT_NET_RESULT);
      STR_CASE(LDCS_MSG_PRELOAD_STAT_NET_RESULT);
      STR_CASE(LDCS_MSG_EXISTS_QUERY);
      STR_CASE(LDCS_MSG_EXISTS_ANSWER);
      STR_CASE(LDCS_MSG_ORIGPATH_QUERY);
      STR_CASE(LDCS_MSG_ORIGPATH_ANSWER);
      STR_CASE(LDCS_MSG_END);
      STR_CASE(LDCS_MSG_CWD);
      STR_CASE(LDCS_MSG_PID);
      STR_CASE(LDCS_MSG_LOCATION);
      STR_CASE(LDCS_MSG_MYRANKINFO_QUERY);
      STR_CASE(LDCS_MSG_MYRANKINFO_QUERY_ANSWER);
      STR_CASE(LDCS_MSG_CACHE_ENTRIES);
      STR_CASE(LDCS_MSG_ACK);
      STR_CASE(LDCS_MSG_DESTROY);
      STR_CASE(LDCS_MSG_FILE_DATA);
      STR_CASE(LDCS_MSG_FILE_DATA_PART);
      STR_CASE(LDCS_MSG_PYTHONPREFIX_REQ);
      STR_CASE(LDCS_MSG_PYTHONPREFIX_RESP);
      STR_CASE(LDCS_MSG_LOADER_DATA_REQ);
      STR_CASE(LDCS_MSG_LOADER_DATA_RESP);
      STR_CASE(LDCS_MSG_LOADER_DATA_NET_REQ);
      STR_CASE(LDCS_MSG_LOADER_DATA_NET_RESP);
      STR_CASE(LDCS_MSG_PRELOAD_LOADER_DATA_NET_RESP);
      STR_CASE(LDCS_MSG_MD_HOSTINFO);
      STR_CASE(LDCS_MSG_MD_HOSTLIST);
      STR_CASE(LDCS_MSG_MD_BOOTSTRAP);
      STR_CASE(LDCS_MSG_MD_BOOTSTRAP_END);
      STR_CASE(LDCS_MSG_MD_BOOTSTRAP_END_OK);
      STR_CASE(LDCS_MSG_MD_BOOTSTRAP_END_NOT_OK);
      STR_CASE(LDCS_MSG_PRELOAD_FILELIST);
      STR_CASE(LDCS_MSG_PRELOAD_DIR);
      STR_CASE(LDCS_MSG_PRELOAD_FILE);
      STR_CASE(LDCS_MSG_PRELOAD_DONE);
      STR_CASE(LDCS_MSG_SELFLOAD_FILE);
      STR_CASE(LDCS_MSG_SETTINGS);
      STR_CASE(LDCS_MSG_EXIT);
      STR_CASE(LDCS_MSG_EXIT_READY);
      STR_CASE(LDCS_MSG_EXIT_CANCEL);
      STR_CASE(LDCS_MSG_BUNDLE);
      STR_CASE(LDCS_MSG_UNKNOWN);
   }
   return "unknown";
}

int ldcs_msg_free(ldcs_message_t **msg) {
  ldcs_message_t *message=*msg;
  if(message->data) {
    free(message->data);  
    message->data=NULL;
  }
  free(message);*msg=NULL;
  return(0);
}

int ldcs_msg_init(ldcs_message_t *msg) {
  msg->header.type=LDCS_MSG_UNKNOWN;
  msg->header.len=0;
  
  msg->data=NULL;
  return(0);
}

ldcs_message_t* ldcs_msg_new() {
  ldcs_message_t *new_msg=NULL;
  new_msg = (ldcs_message_t *) malloc(sizeof(ldcs_message_t));
  if (!new_msg)  _error("could not allocate memory for message");
  ldcs_msg_init(new_msg);
  return(new_msg);
}

ldcs_message_t* ldcs_msg_copy(ldcs_message_t *msg) {
  ldcs_message_t *new_msg=NULL;
  new_msg = (ldcs_message_t *) malloc(sizeof(ldcs_message_t));
  if (!new_msg)  _error("could not allocate memory for message");
  new_msg->header.type   = msg->header.type;
  new_msg->header.len    = msg->header.len;
  if(new_msg->header.len>0) {
    new_msg->data = (char *) malloc(new_msg->header.len);
    if (!new_msg->data)  _error("could not allocate memory for message data");
    memcpy(new_msg->data,msg->data,msg->header.len);
  } else {
    new_msg->data=NULL;
  }
  return(new_msg);
}


void _error(const char *msg)
{
    perror(msg);
    exit(1);
}

char* ldcs_substring(const char* str, size_t begin, size_t len)
{
  if (str == 0 || strlen(str) == 0 || strlen(str) < begin || strlen(str) < (begin+len))
    return 0;

  return strndup(str + begin, len);
} 

/**
   create a new string just big enough to hold the string t and copy
   t to it. a check for memory problems is included
*/
char *ldcs_new_char(const char *t) {
  char *tmp;

  if(t == NULL) { return NULL; }

  if((tmp = malloc(strlen(t) + 1)) == NULL ) {
    return NULL;
  }
  strcpy(tmp, t);

  return tmp;
}

int ldcs_dump_memmaps(int pid) {
  int rc=0;
  char filename[MAX_PATH_LEN];
  FILE * fp;
  char * line = NULL;
  size_t len = 0;
  ssize_t read;

  sprintf(filename,"/proc/%d/maps",pid);

  fp = fopen(filename, "r");   if (fp == NULL)  perror("could not open proc maps file");
  
  while( (read = getline(&line, &len, fp)) >= 0) {
    debug_printf3("%s: %s",filename,line);
    printf("%s: %s",filename,line);
  }
  fclose(fp);
  if (line) free(line);
  
  return(rc);
}

double ldcs_get_time() {
  struct timeval tp;
  gettimeofday (&tp, (struct timezone *)NULL);
  return tp.tv_sec + tp.tv_usec/1000000.0;
}
