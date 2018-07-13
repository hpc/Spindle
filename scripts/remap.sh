#!/bin/bash

WORKINGDIR=$1
SRCDIR=$2
CCLINE=$3
UNIQ=$4

cat <<EOF | $CCLINE -I $SRCDIR -shared -DWORKINGDIR=\"$WORKINGDIR\" -DUNIQ=\"$UNIQ\" -o $WORKINGDIR/libremap$UNIQ.so -fPIC -x c -
#define REMAP_CONF_TEST

#define debug_printf printf
#define debug_printf2 printf
#define debug_printf3 printf
#define err_printf printf
#define spindle_free free
#define MAX_PATH_LEN 4096
#include <string.h>

void get_relocated_file(int ldcs_id, char *orig_exec, char **reloc_exec, int *errcode)
{
   *reloc_exec = strdup(WORKINGDIR "/remap2" UNIQ);
   *errcode = 0;
}

#include "src/client/client/remap_exec.c"

int dowork()
{
   char buffer[4097];
   buffer[4096] = '\0';
   remap_executable(0);
   if (readlink("/proc/self/exe", buffer, 4096) == -1)
      return -1;
   if (strstr(buffer, "remap2"))
      return -1;
   return 0;
}
EOF

cat <<EOF | $CCLINE -o $WORKINGDIR/remap$UNIQ -L$WORKINGDIR -lremap$UNIQ -Wl,-rpath,$WORKINGDIR -x c -
extern int dowork();
int data = 5;
int main(int argc, char *argv[])
{
   return dowork();
}
EOF

cp $WORKINGDIR/remap$UNIQ $WORKINGDIR/remap2$UNIQ
$WORKINGDIR/remap$UNIQ > /dev/null
RETVAL=$?
rm -f $WORKINGDIR/remap$UNIQ $WORKINGDIR/remap2$UNIQ $WORKINGDIR/libremap$UNIQ.so
exit $RETVAL
