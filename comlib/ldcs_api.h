#ifndef LDCS_API_H
#define LDCS_API_H

/* messages structure */
typedef enum {
   LDCS_MSG_FILE_QUERY,
   LDCS_MSG_FILE_QUERY_ANSWER,
   LDCS_MSG_FILE_QUERY_EXACT_PATH,
   LDCS_MSG_END,
   LDCS_MSG_CWD,
   LDCS_MSG_HOSTNAME,
   LDCS_MSG_PID,
   LDCS_MSG_LOCATION,
   LDCS_MSG_MYRANKINFO_QUERY,
   LDCS_MSG_MYRANKINFO_QUERY_ANSWER,
   LDCS_MSG_CACHE_ENTRIES,
   LDCS_MSG_ACK,
   LDCS_MSG_DESTROY,
   LDCS_MSG_FILE_DATA,
   LDCS_MSG_FILE_DATA_PART,
   LDCS_MSG_MD_HOSTINFO,
   LDCS_MSG_MD_HOSTLIST,
   LDCS_MSG_MD_BOOTSTRAP,
   LDCS_MSG_MD_BOOTSTRAP_END,
   LDCS_MSG_MD_BOOTSTRAP_END_OK,
   LDCS_MSG_MD_BOOTSTRAP_END_NOT_OK,
   LDCS_MSG_PRELOAD_FILE,
   LDCS_MSG_PRELOAD_FILE_OK,
   LDCS_MSG_PRELOAD_FILE_NOT_FOUND,
   LDCS_MSG_UNKNOWN,
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
  int   source; 		/* rank of server which create msg */
  int   from; 			/* rank of server which sending msg to me */
  int   dest;			/* destination of msg */
  int   mtype;
  ldcs_msg_mtype_t mdir;
};
typedef struct ldcs_message_header_struct ldcs_message_header_t;

struct ldcs_message_struct
{
  ldcs_message_header_t header;
  int   alloclen;
  char *data;
};
typedef struct ldcs_message_struct ldcs_message_t;

/* client */
int ldcs_open_connection(char* location, int number);
int ldcs_close_connection(int connid);

int ldcs_send_msg(int connid, ldcs_message_t * msg);
ldcs_message_t * ldcs_recv_msg(int fd, ldcs_read_block_t block);
int ldcs_recv_msg_static(int fd, ldcs_message_t *msg, ldcs_read_block_t block);

int ldcs_msg_init(ldcs_message_t *msg);
int ldcs_msg_free(ldcs_message_t **msg);
ldcs_message_t* ldcs_msg_new();
ldcs_message_t* ldcs_msg_copy(ldcs_message_t *msg);

/* server */
int ldcs_create_server(char* location, int number);
int ldcs_open_server_connection(int serverid);
int ldcs_open_server_connections(int fd, int *more_avail);
int ldcs_close_server_connection(int connid);
int ldcs_destroy_server(int cid);
int ldcs_select(int serverid);

/* shortcut functions */
int ldcs_send_FILE_QUERY (int fd, char* path, char** newpath);
int ldcs_send_FILE_QUERY_EXACT_PATH (int fd, char* path, char** newpath);
int ldcs_send_CWD (int fd);
int ldcs_send_END (int fd);
int ldcs_send_HOSTNAME (int fd);
int ldcs_send_PID (int fd);
int ldcs_send_LOCATION (int fd, char *location);
int ldcs_send_MYRANKINFO_QUERY (int fd, int *mylrank, int *mylsize, int *mymdrank, int *mymdsize);

/* get info */
int ldcs_get_fd (int fd);

/* internal */
char* _message_type_to_str (ldcs_message_ids_t type);
void  _error(const char *msg);
int   _ldcs_mkfifo(char *fifo);
char* ldcs_substring(const char* str, size_t begin, size_t len);
char *ldcs_new_char(const char *t);

/* time measurement */
double ldcs_get_time();


#if defined(DEBUG)
#define LDCSDEBUG 1
#define debug_printf(format, ...) \
  do { \
     fprintf(stderr, "[%s:%u@%d] - " format, __FILE__, __LINE__, getpid(), ## __VA_ARGS__); \
  } while (0)
#elif defined(SIONDEBUG)
#define LDCSDEBUG 1
#include "sion_debug.h"
#define debug_printf(format, ...) \
  do { \
    sion_dprintfp(32, __FILE__, getpid(), "[L%04u, %12.2f] - " format, __LINE__,_sion_get_time(), ## __VA_ARGS__); \
  } while (0)
#else
#define debug_printf(format, ...)
#endif

#define MAX_MSG_PRINT_SIZE 50
#define MAX_PATH_LEN 1024

#endif
