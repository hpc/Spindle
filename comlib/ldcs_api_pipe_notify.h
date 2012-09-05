#ifndef LDCS_API_PIPE_NOTIFY_H
#define LDCS_API_PIPE_NOTIFY_H

int ldcs_notify_init(char *path);
int ldcs_notify_destroy(int fd);
int ldcs_notify_get_fd(int fd);
char *ldcs_notify_get_next_file(int nfd);
int ldcs_notify_more_avail(int fd);

#endif
