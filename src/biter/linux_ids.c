#include "ids.h"
#include <unistd.h>

extern int get_id(int session_id);

int biterc_get_job_id()
{
   return 0;
}

int biterc_get_unique_number(int *mrank, int *num_set, void *session)
{
   return 0;
}

unsigned int biterc_get_rank(int session_id)
{
   return get_id(session_id);
}
