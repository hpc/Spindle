#ifndef LDCS_API_PIPE_H
#define LDCS_API_PIPE_H

int ldcs_open_connection_pipe(char* location, int number);
int ldcs_close_connection_pipe(int fd);
int ldcs_create_server_pipe(char* location, int number);
int ldcs_open_server_connection_pipe(int fd);
int ldcs_open_server_connections_pipe(int fd, int *more_avail);
int ldcs_close_server_connection_pipe(int fd);
int ldcs_get_fd_pipe(int id);
int ldcs_destroy_server_pipe(int fd);
int ldcs_send_msg_pipe(int fd, ldcs_message_t * msg);
ldcs_message_t * ldcs_recv_msg_pipe(int fd, ldcs_read_block_t block );
int ldcs_recv_msg_static_pipe(int fd, ldcs_message_t *msg, ldcs_read_block_t block);

/* internal */
int _ldcs_write_pipe(int fd, const void *data, int bytes );
int _ldcs_read_pipe(int fd, void *data, int bytes, ldcs_read_block_t block );

#endif

