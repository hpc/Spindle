#include <link.h>
#include <elf.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#if !defined(LIBEXECDIR)
#error LIBEXECDIR must be defined
#endif

int main(int argc, char *argv[])
{
   ElfW(Dyn) *dyn;
   ElfW(Addr) *pltgot = NULL;
   char path[4097];

   for (dyn = _DYNAMIC; dyn->d_tag != DT_NULL; dyn++) {
      if (dyn->d_tag == DT_PLTGOT) {
         pltgot = (ElfW(Addr) *) dyn->d_un.d_ptr;
         break;
      }
   }
   if (!pltgot)
      return -1;

   printf("%lu\n", pltgot[2] - _r_debug.r_ldbase);

   if (getenv("LD_AUDIT"))
      return 0;

   snprintf(path, sizeof(path), "%s/%s", LIBEXECDIR, "libprint_ldso_audit.so");
   path[sizeof(path)-1] = '\0';
   setenv("LD_AUDIT", path, 1);
   
   execv(argv[1], argv+1);

   return 0;
}
