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
char* _message_type_to_str (ldcs_message_ids_t type) {

  return(
	 (type == LDCS_MSG_FILE_QUERY)        ? "LDCS_MSG_FILE_QUERY" :
	 (type == LDCS_MSG_FILE_QUERY_ANSWER) ? "LDCS_MSG_FILE_QUERY_ANSWER" :
	 (type == LDCS_MSG_FILE_QUERY_EXACT_PATH) ? "LDCS_MSG_FILE_QUERY_EXACT_PATH" :
	 (type == LDCS_MSG_MYRANKINFO_QUERY)        ? "LDCS_MSG_MYRANKINFO_QUERY" :
	 (type == LDCS_MSG_MYRANKINFO_QUERY_ANSWER)        ? "LDCS_MSG_MYRANKINFO_QUERY_ANSWER" :
	 (type == LDCS_MSG_CACHE_ENTRIES)     ? "LDCS_MSG_CACHE_ENTRIES" :
	 (type == LDCS_MSG_ACK)               ? "LDCS_MSG_ACK" :
	 (type == LDCS_MSG_FILE_DATA)         ? "LDCS_MSG_FILE_DATA" :
	 (type == LDCS_MSG_FILE_DATA_PART)    ? "LDCS_MSG_FILE_DATA_PART" :
	 (type == LDCS_MSG_END)               ? "LDCS_MSG_END" :
	 (type == LDCS_MSG_CWD)               ? "LDCS_MSG_CWD" :
	 (type == LDCS_MSG_PID)               ? "LDCS_MSG_PID" :
	 (type == LDCS_MSG_HOSTNAME)          ? "LDCS_MSG_HOSTNAME" :
	 (type == LDCS_MSG_LOCATION)          ? "LDCS_MSG_LOCATION" :
	 (type == LDCS_MSG_DESTROY)           ? "LDCS_MSG_DESTROY" :
	 (type == LDCS_MSG_MD_HOSTINFO)       ? "LDCS_MSG_MD_HOSTINFO" :
	 (type == LDCS_MSG_MD_HOSTLIST)       ? "LDCS_MSG_MD_HOSTLIST" :
	 (type == LDCS_MSG_MD_BOOTSTRAP)      ? "LDCS_MSG_MD_BOOTSTRAP" :
	 (type == LDCS_MSG_MD_BOOTSTRAP_END)      ? "LDCS_MSG_MD_BOOTSTRAP_END" :
	 (type == LDCS_MSG_MD_BOOTSTRAP_END_OK)      ? "LDCS_MSG_MD_BOOTSTRAP_END_OK" :
	 (type == LDCS_MSG_MD_BOOTSTRAP_END_NOT_OK)      ? "LDCS_MSG_MD_BOOTSTRAP_END_NOT_OK" :
	 (type == LDCS_MSG_PRELOAD_FILE)      ? "LDCS_MSG_PRELOAD_FILE" :
	 (type == LDCS_MSG_PRELOAD_FILE_OK)      ? "LDCS_MSG_PRELOAD_FILE_OK" :
	 (type == LDCS_MSG_PRELOAD_FILE_NOT_FOUND)      ? "LDCS_MSG_PRELOAD_FILE_NOT_FOUND" :
    (type == LDCS_MSG_EXIT)              ? "LDCS_MSG_EXIT" :
	 (type == LDCS_MSG_UNKNOWN)           ? "LDCS_MSG_UNKNOWN" :
	 "???");
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
  msg->header.source=-1;
  msg->header.dest=-1;
  msg->header.mtype=LDCS_MSG_MTYPE_UNKNOWN;
  msg->header.mdir=LDCS_MSG_MDIR_UNKNOWN;
  msg->alloclen=0;
  
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
  new_msg->header.source = msg->header.source;
  new_msg->header.dest   = msg->header.dest;
  new_msg->header.mtype  = msg->header.mtype;
  new_msg->header.mdir   = msg->header.mdir;
  if(new_msg->header.len>0) {
    new_msg->data = (char *) malloc(sizeof(new_msg->header.len));
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
