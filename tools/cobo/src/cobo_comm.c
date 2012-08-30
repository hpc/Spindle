#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <netdb.h>
#include <errno.h>
#include <stdarg.h>
#include <poll.h>
#include <fcntl.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <poll.h>
#include "cobo.h"

/* print message to stderr */
static void ldcs_cobo_error(char *fmt, ...)
{
    va_list argp;
    char hostname[256];
    gethostname(hostname, 256);
    fprintf(stderr, "COBO ERROR: ");
    fprintf(stderr, "rank %d on %s: ", -1, hostname);
    va_start(argp, fmt);
    vfprintf(stderr, fmt, argp);
    va_end(argp);
    fprintf(stderr, "\n");
}

/* print message to stderr */
static void ldcs_cobo_warn(char *fmt, ...)
{
    va_list argp;
    char hostname[256];
    gethostname(hostname, 256);
    fprintf(stderr, "COBO WARNING: ");
    fprintf(stderr, "rank %d on %s: ", -1, hostname);
    va_start(argp, fmt);
    vfprintf(stderr, fmt, argp);
    va_end(argp);
    fprintf(stderr, "\n");
}

/* print message to stderr */
static void ldcs_cobo_debug(int level, char *fmt, ...)
{
    va_list argp;
    char hostname[256];
    gethostname(hostname, 256);
    fprintf(stderr, "COBO DEBUG: ");
    fprintf(stderr, "rank %d on %s: ", -1, hostname);
    va_start(argp, fmt);
    vfprintf(stderr, fmt, argp);
    va_end(argp);
    fprintf(stderr, "\n");
}

/* read size bytes into buf from fd, retry if necessary */
static int ldcs_cobo_read_fd_w_timeout(int fd, void* buf, int size, int usecs)
{
    int rc;
    int n = 0;
    char* offset = (char*) buf;

    struct pollfd fds;
    fds.fd      = fd;
    fds.events  = POLLIN;
    fds.revents = 0x0;

    while (n < size) {
        /* poll the connection with a timeout value */
        int poll_rc = poll(&fds, 1, usecs);
        if (poll_rc < 0) {
            ldcs_cobo_error("Polling file descriptor for read (read(fd=%d,offset=%x,size=%d) %m errno=%d) @ file %s:%d",
                       fd, offset, size-n, errno, __FILE__, __LINE__
            );
            return -1;
        } else if (poll_rc == 0) {
            return -1;
        }

        /* check the revents field for errors */
        if (fds.revents & POLLHUP) {
            ldcs_cobo_debug(1, "Hang up error on poll for read(fd=%d,offset=%x,size=%d) @ file %s:%d",
                       fd, offset, size-n, __FILE__, __LINE__
            );
            return -1;
        }

        if (fds.revents & POLLERR) {
            ldcs_cobo_debug(1, "Error on poll for read(fd=%d,offset=%x,size=%d) @ file %s:%d",
                       fd, offset, size-n, __FILE__, __LINE__
            );
            return -1;
        }

        if (fds.revents & POLLNVAL) {
            ldcs_cobo_error("Invalid request on poll for read(fd=%d,offset=%x,size=%d) @ file %s:%d",
                       fd, offset, size-n, __FILE__, __LINE__
            );
            return -1;
        }

        if (!(fds.revents & POLLIN)) {
            ldcs_cobo_error("No errors found, but POLLIN is not set for read(fd=%d,offset=%x,size=%d) @ file %s:%d",
                       fd, offset, size-n, __FILE__, __LINE__
            );
            return -1;
        }

        /* poll returned that fd is ready for reading */
	rc = read(fd, offset, size - n);

	if (rc < 0) {
	    if(errno == EINTR || errno == EAGAIN) { continue; }
            ldcs_cobo_error("Reading from file descriptor (read(fd=%d,offset=%x,size=%d) %m errno=%d) @ file %s:%d",
                       fd, offset, size-n, errno, __FILE__, __LINE__
            );
	    return rc;
	} else if(rc == 0) {
            ldcs_cobo_error("Unexpected return code of 0 from read from file descriptor (read(fd=%d,offset=%x,size=%d) revents=%x) @ file %s:%d",
                       fd, offset, size-n, fds.revents, __FILE__, __LINE__
            );
	    return -1;
	}

	offset += rc;
	n += rc;
    }

    return n;
}

/* read size bytes into buf from fd, retry if necessary */
int ldcs_cobo_read_fd(int fd, void* buf, int size)
{
    return ldcs_cobo_read_fd_w_timeout(fd, buf, size, -1);
}

/* write size bytes from buf into fd, retry if necessary */
static int ldcs_cobo_write_fd_w_suppress(int fd, void* buf, int size, int suppress)
{
    int rc;
    int n = 0;
    char* offset = (char*) buf;

    while (n < size) {
	rc = write(fd, offset, size - n);

	if (rc < 0) {
	    if(errno == EINTR || errno == EAGAIN) { continue; }
            if (!suppress) {
                ldcs_cobo_error("Writing to file descriptor (write(fd=%d,offset=%x,size=%d) %m errno=%d) @ file %s:%d",
                           fd, offset, size-n, errno, __FILE__, __LINE__
                );
            } else {
                ldcs_cobo_debug(1, "Writing to file descriptor (write(fd=%d,offset=%x,size=%d) %m errno=%d) @ file %s:%d",
                           fd, offset, size-n, errno, __FILE__, __LINE__
                );
            }
	    return rc;
	} else if(rc == 0) {
            if (!suppress) {
                ldcs_cobo_error("Unexpected return code of 0 from write to file descriptor (write(fd=%d,offset=%x,size=%d)) @ file %s:%d",
                           fd, offset, size-n, __FILE__, __LINE__
                );
            } else {
                ldcs_cobo_debug(1, "Unexpected return code of 0 from write to file descriptor (write(fd=%d,offset=%x,size=%d)) @ file %s:%d",
                           fd, offset, size-n, __FILE__, __LINE__
                );
            }
	    return -1;
	}

	offset += rc;
	n += rc;
    }

    return n;
}

/* write size bytes from buf into fd, retry if necessary */
int ldcs_cobo_write_fd(int fd, void* buf, int size)
{
    return ldcs_cobo_write_fd_w_suppress(fd, buf, size, 0);
}
