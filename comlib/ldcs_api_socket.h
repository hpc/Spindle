#ifndef LDCS_API_SOCKET_H
#define LDCS_API_SOCKET_H

int ldcs_open_connection_socket(char* location, int number);
int ldcs_close_connection_socket(int fd);
int ldcs_create_server_socket(char* location, int number);
int ldcs_open_server_connection_socket(int fd);
int ldcs_open_server_connections_socket(int fd, int *more_avail);
int ldcs_close_server_connection_socket(int fd);
int ldcs_get_fd_socket(int id);
int ldcs_destroy_server_socket(int fd);
int ldcs_send_msg_socket(int fd, ldcs_message_t * msg);
ldcs_message_t * ldcs_recv_msg_socket(int fd, ldcs_read_block_t block );
int ldcs_recv_msg_static_socket(int fd, ldcs_message_t *msg, ldcs_read_block_t block);

/* internal */
int _ldcs_write_socket(int fd, const void *data, int bytes );
int _ldcs_read_socket(int fd, void *data, int bytes, ldcs_read_block_t block );

#endif

