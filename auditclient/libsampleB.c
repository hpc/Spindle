#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>
#include <stddef.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>
#include <assert.h>
#include <stdio.h>

int fooB () {
  printf("This is function foo B\n");
  return(0);
}

int barB () {
  printf("This is function bar B\n");
  return(0);
}
