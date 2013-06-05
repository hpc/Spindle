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

#ifndef LDCS_API_H
#define LDCS_API_H

#include "spindle_debug.h"

/* messages structure */
typedef enum {
   LDCS_MSG_FILE_QUERY,
   LDCS_MSG_FILE_QUERY_ANSWER,
   LDCS_MSG_FILE_QUERY_EXACT_PATH,
   LDCS_MSG_FILE_REQUEST,
   LDCS_MSG_STAT_QUERY,
   LDCS_MSG_STAT_ANSWER,
   LDCS_MSG_STAT_NET_REQUEST,
   LDCS_MSG_STAT_NET_RESULT,
   LDCS_MSG_EXISTS_QUERY,
   LDCS_MSG_EXISTS_ANSWER,
   LDCS_MSG_END,
   LDCS_MSG_CWD,
   LDCS_MSG_PID,
   LDCS_MSG_LOCATION,
   LDCS_MSG_MYRANKINFO_QUERY,
   LDCS_MSG_MYRANKINFO_QUERY_ANSWER,
   LDCS_MSG_CACHE_ENTRIES,
   LDCS_MSG_ACK,
   LDCS_MSG_DESTROY,
   LDCS_MSG_FILE_DATA,
   LDCS_MSG_FILE_DATA_PART,
   LDCS_MSG_PYTHONPREFIX_REQ,
   LDCS_MSG_PYTHONPREFIX_RESP,
   LDCS_MSG_MD_HOSTINFO,
   LDCS_MSG_MD_HOSTLIST,
   LDCS_MSG_MD_BOOTSTRAP,
   LDCS_MSG_MD_BOOTSTRAP_END,
   LDCS_MSG_MD_BOOTSTRAP_END_OK,
   LDCS_MSG_MD_BOOTSTRAP_END_NOT_OK,
   LDCS_MSG_PRELOAD_FILELIST,
   LDCS_MSG_PRELOAD_DIR,
   LDCS_MSG_PRELOAD_FILE,
   LDCS_MSG_PRELOAD_DONE,
   LDCS_MSG_SELFLOAD_FILE,
   LDCS_MSG_EXIT,
   LDCS_MSG_UNKNOWN
} ldcs_message_ids_t;

typedef  enum {
   LDCS_READ_BLOCK,
   LDCS_READ_NO_BLOCK,
   LDCS_READ_UNKNOWN
} ldcs_read_block_t;

typedef  enum {
   LDCS_MSG_MTYPE_P2P,
   LDCS_MSG_MTYPE_BCAST,
   LDCS_MSG_MTYPE_BARRIER,
   LDCS_MSG_MTYPE_UNKNOWN
} ldcs_msg_mtype_t;

typedef  enum {
   LDCS_MSG_MDIR_UP,
   LDCS_MSG_MDIR_DOWN,
   LDCS_MSG_MDIR_UNKNOWN
} ldcs_msg_mdir_t;


/* source, dest: -1: frontend 0...10000: MD server, > 10000 local client   */
struct ldcs_message_header_struct
{
  ldcs_message_ids_t type;
  int   len;
};
typedef struct ldcs_message_header_struct ldcs_message_header_t;

struct ldcs_message_struct
{
  ldcs_message_header_t header;
  char *data;
};
typedef struct ldcs_message_struct ldcs_message_t;

int ldcs_send_msg(int connid, ldcs_message_t * msg);
ldcs_message_t * ldcs_recv_msg(int fd, ldcs_read_block_t block);
int ldcs_recv_msg_static(int fd, ldcs_message_t *msg, ldcs_read_block_t block);

int ldcs_create_server(char* location, int number);
int ldcs_open_server_connection(int serverid);
int ldcs_open_server_connections(int fd, int *more_avail);
int ldcs_close_server_connection(int connid);
int ldcs_destroy_server(int cid);
int ldcs_select(int serverid);

int ldcs_msg_init(ldcs_message_t *msg);
int ldcs_msg_free(ldcs_message_t **msg);
ldcs_message_t* ldcs_msg_new();
ldcs_message_t* ldcs_msg_copy(ldcs_message_t *msg);

/* get info */
int ldcs_get_fd (int fd);

/* internal */
char* _message_type_to_str (ldcs_message_ids_t type);
void  _error(const char *msg);
char* ldcs_substring(const char* str, size_t begin, size_t len);
char *ldcs_new_char(const char *t);

/* time measurement */
double ldcs_get_time();

/* Force exit */
void mark_exit();

#define MAX_PATH_LEN 4096
#define MAX_NAME_LEN 255
#endif
