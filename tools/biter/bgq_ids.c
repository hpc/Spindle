#include <stdint.h>
#include <limits.h>
#include "spi/include/kernel/location.h"
#include "spi/include/kernel/process.h"
#include "ids.h"

#define MEMORY_BARRIER __sync_synchronize()

extern int take_queue_lock(void *session);
extern int release_queue_lock(void *session);

int biterc_get_job_id()
{
   int result = (int) Kernel_GetJobID();
   if (result == INT_MIN)
      result = INT_MAX;
   else if (result < 0)
      result *= -1;
   return result;
}

unsigned int biterc_get_rank(int session_id)
{
   return Kernel_GetRank();
}

int biterc_get_unique_number(int *mrank, int *num_set, void *session)
{
   int result;
   uint32_t num_procs, my_rank;
   int queue_lock_held = 0;
   volatile uint32_t *max_rank = (uint32_t *) mrank;
   volatile int *num_set_max_rank = num_set;

   num_procs = Kernel_ProcessCount();
   my_rank = Kernel_GetRank();

   result = take_queue_lock(session);
   if (result == -1)
      return -1;
   queue_lock_held = 1;

   if (*max_rank < my_rank)
      *max_rank = my_rank;

   MEMORY_BARRIER;

   (*num_set_max_rank)++;

   result = release_queue_lock(session);
   if (result == -1)
      goto error;
   queue_lock_held = 0;

   while (*num_set_max_rank < num_procs);

   return (int) (*max_rank);
   
  error:
   if (queue_lock_held)
      release_queue_lock(session);
   return -1;
}
