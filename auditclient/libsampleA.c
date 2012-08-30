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

int fooA () {
  printf("This is function foo A\n");
}

int barA () {
  printf("This is function bar A\n");
}
