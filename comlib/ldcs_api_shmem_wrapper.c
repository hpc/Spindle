#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <sys/syscall.h>

#include "ldcs_api.h"
#include "ldcs_api_shmem.h"

int ldcs_open_connection(char* location, int number) {
  return(ldcs_open_connection_shmem(location,number));
}

char *ldcs_get_connection_string(int fd) {
   return ldcs_get_connection_string_shmem(fd);
}

int ldcs_register_connection(char *connection_str) {
   return ldcs_register_connection_shmem(connection_str);
}

int ldcs_close_connection(int fd) {
  return(ldcs_close_connection_shmem(fd));
}

int ldcs_create_server(char* location, int number) {
  return(ldcs_create_server_shmem(location,number));
}

int ldcs_open_server_connection(int fd) {
  return(ldcs_open_server_connection_shmem(fd));
}

int ldcs_open_server_connections(int fd, int *more_avail) {
  return(ldcs_open_server_connections_shmem(fd,more_avail));
}

int ldcs_close_server_connection(int fd) {
  return(ldcs_close_server_connection_shmem(fd));
}

int ldcs_get_fd(int id) {
  return(ldcs_get_fd_shmem(id));
}

int ldcs_destroy_server(int fd) {
  return(ldcs_destroy_server_shmem(fd));
}

int ldcs_send_msg(int fd, ldcs_message_t * msg) {
  return(ldcs_send_msg_shmem(fd,msg));
}

ldcs_message_t * ldcs_recv_msg(int fd, ldcs_read_block_t block ) {
  return(ldcs_recv_msg_shmem(fd,block));
}

int ldcs_recv_msg_static(int fd, ldcs_message_t *msg, ldcs_read_block_t block) {
  return(ldcs_recv_msg_static_shmem(fd, msg, block));
}
