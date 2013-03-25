#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <sys/syscall.h>

#include "ldcs_api.h"
#include "ldcs_api_socket.h"

int ldcs_open_connection(char* location, int number) {
  return(ldcs_open_connection_socket(location,number));
}

char *ldcs_get_connection_string(int fd) {
   return ldcs_get_connection_string_socket(fd);
}

int ldcs_register_connection(char *connection_str) {
   return ldcs_register_connection_socket(connection_str);
}

int ldcs_close_connection(int fd) {
  return(ldcs_close_connection_socket(fd));
}

int ldcs_create_server(char* location, int number) {
  return(ldcs_create_server_socket(location,number));
}

int ldcs_open_server_connection(int fd) {
  return(ldcs_open_server_connection_socket(fd));
}

int ldcs_open_server_connections(int fd, int *more_avail) {
  return(ldcs_open_server_connections_socket(fd,more_avail));
}

int ldcs_close_server_connection(int fd) {
  return(ldcs_close_server_connection_socket(fd));
}

int ldcs_get_fd(int id) {
  return(ldcs_get_fd_socket(id));
}

int ldcs_destroy_server(int fd) {
  return(ldcs_destroy_server_socket(fd));
}

int ldcs_send_msg(int fd, ldcs_message_t * msg) {
  return(ldcs_send_msg_socket(fd,msg));
}

ldcs_message_t * ldcs_recv_msg(int fd, ldcs_read_block_t block ) {
  return(ldcs_recv_msg_socket(fd,block));
}

int ldcs_recv_msg_static(int fd, ldcs_message_t *msg, ldcs_read_block_t block) {
  return(ldcs_recv_msg_static_socket(fd, msg, block));
}
