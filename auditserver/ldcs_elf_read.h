#if !defined(LDCS_ELF_READ_H_)
#define LDCS_ELF_READ_H_

#include <stdio.h>

int read_file_and_strip(FILE *f, void *data, size_t *size);

#endif
