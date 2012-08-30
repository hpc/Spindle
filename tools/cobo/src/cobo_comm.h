#ifndef _COBO_COMM_H
#define _COBO_COMM_H

#define COBO_SUCCESS (0)

int ldcs_cobo_read_fd(int fd, void* buf, int size);
int ldcs_cobo_write_fd(int fd, void* buf, int size);



#endif /* _COBO_COMM_H */
