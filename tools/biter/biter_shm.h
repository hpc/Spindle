#if !defined(BITER_SHM_H_)
#define BITER_SHM_H_

/**
 * We try to keep the client library only dependant on glibc (because
 * anything it depends on must be loaded via traditional file system
 * access).  Unfortunately, the shm_open and shm_unlink functions
 * come from librt.so.  So we've got our own implementations.
 **/
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int biter_shm_open(const char *name, int oflag, mode_t mode);
int biter_shm_unlink(const char *name);

#endif
