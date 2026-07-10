#include <stdlib.h>

__attribute__((constructor))
static void ctor_crash(void)
{
    *(volatile int *) 0 = 0;
}
