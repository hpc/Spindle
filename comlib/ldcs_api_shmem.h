#ifndef LDCS_API_SHMEM_H
#define LDCS_API_SHMEM_H

int ldcs_open_connection_shmem(char* location, int number);
int ldcs_close_connection_shmem(int fd);
int ldcs_create_server_shmem(char* location, int number);
int ldcs_open_server_connection_shmem(int fd);
int ldcs_open_server_connections_shmem(int fd, int *more_avail);
int ldcs_close_server_connection_shmem(int fd);
int ldcs_get_fd_shmem(int id);
int ldcs_destroy_server_shmem(int fd);
int ldcs_send_msg_shmem(int fd, ldcs_message_t * msg);
ldcs_message_t * ldcs_recv_msg_shmem(int fd, ldcs_read_block_t block );
int ldcs_recv_msg_static_shmem(int fd, ldcs_message_t *msg, ldcs_read_block_t block);

/* internal */
int _ldcs_write_shmem(int fd, const void *data, int bytes );
int _ldcs_read_shmem(int fd, void *data, int bytes, ldcs_read_block_t block );

/* additional API functions for SHMEM */
int ldcs_create_server_or_client_shmem(char* location, int number);
int ldcs_is_server_shmem(int ldcsid);
int ldcs_is_client_shmem(int ldcsid);

#endif

