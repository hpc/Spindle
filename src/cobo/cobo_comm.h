#ifndef _COBO_COMM_H
#define _COBO_COMM_H

#define COBO_SUCCESS (0)

#include "ldcs_api.h"

int ldcs_cobo_read_fd(int fd, void* buf, int size);
int ldcs_cobo_write_fd(int fd, void* buf, int size);
int ll_write(int fd, void *buf, size_t count);
int write_msg(int fd, ldcs_message_t *msg);

#endif /* _COBO_COMM_H */
