#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <alloca.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#if !defined(DEV_SHM)
#define DEV_SHM "/dev/shm/"
#endif

#if !defined(USE_GLIBC_SHM)

int biter_shm_open(const char *name, int oflag, mode_t mode)
{
   size_t name_size, dev_shm_size, new_name_size;
   char *new_name;
   int fd;

   while (*name == '/') name++;
   if (*name == '\0') {
      errno = EINVAL;
      return -1;
   }

   name_size = strlen(name);
   dev_shm_size = strlen(DEV_SHM);
   new_name_size = name_size + dev_shm_size + 1;
   if (name_size > 255) {
      errno = EINVAL;
      return -1;
   }
   
   new_name = alloca(new_name_size);
   memcpy(new_name, DEV_SHM, dev_shm_size);
   memcpy(new_name+dev_shm_size, name, name_size+1);
   
   fd = open(new_name, oflag | O_NOFOLLOW | O_CLOEXEC, mode);
   if (fd == -1)
      return -1;

   return fd;
}

int biter_shm_unlink(const char *name)
{
   size_t name_size, dev_shm_size, new_name_size;
   char *new_name;

   while (*name == '/') name++;
   if (*name == '\0') {
      errno = EINVAL;
      return -1;
   }

   name_size = strlen(name);
   dev_shm_size = strlen(DEV_SHM);
   new_name_size = name_size + dev_shm_size + 1;
   if (name_size > 255) {
      errno = EINVAL;
      return -1;
   }
   
   new_name = alloca(new_name_size);
   memcpy(new_name, DEV_SHM, dev_shm_size);
   memcpy(new_name+dev_shm_size, name, name_size+1);

   return unlink(new_name);
}

#else

int biter_shm_open(const char *name, int oflag, mode_t mode)
{
   return shm_open(name, oflag, mode);
}

int biter_shm_unlink(const char *name)
{
   return shm_unlink(name);
}

#endif
