#include <stdlib.h>
#include <assert.h>
#include "daemon_ids.h"

int biterd_num_compute_nodes()
{
   return 1;
}

int biterd_ranks_in_cn(int cn_id)
{
   char *proc_s = getenv("PROCS");
   if (proc_s)
      return atoi(proc_s);
      
   assert(0);
}

int biterd_unique_num_for_cn(int cn_id)
{
   return 0;
}

int biterd_get_rank(int compute_node_id, int client_id)
{
   return client_id;
}

int biterd_register_rank(int session_id, uint32_t client_id, uint32_t rank)
{
   return 0;
}
