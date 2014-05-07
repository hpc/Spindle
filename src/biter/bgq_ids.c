/*
This file is part of Spindle.  For copyright information see the COPYRIGHT 
file in the top level directory, or at 
https://github.com/hpc/Spindle/blob/master/COPYRIGHT

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License (as published by the Free Software
Foundation) version 2.1 dated February 1999.  This program is distributed in the
hope that it will be useful, but WITHOUT ANY WARRANTY; without even the IMPLIED
WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms 
and conditions of the GNU General Public License for more details.  You should 
have received a copy of the GNU Lesser General Public License along with this 
program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include <stdint.h>
#include <limits.h>
#include "spi/include/kernel/location.h"
#include "spi/include/kernel/process.h"
#include "ids.h"
#include "spindle_debug.h"

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
