/*
This file is part of Spindle.  For copyright information see the COPYRIGHT 
file in the top level directory, or at 
https://github.com/hpc/Spindle/blob/master/COPYRIGHT

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License (as published by the Free Software
Foundation) version 2.1 dated February 1999.  This program is distributed in the
hope that it will be useful, but WITHOUT ANY WARRANTY; without even the IMPLIED
WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms 
and conditions of the GNU Lesser General Public License for more details.  You should 
have received a copy of the GNU Lesser General Public License along with this 
program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include "config.h"

#if defined(LIBNUMA)

#include <numa.h>
#include <numaif.h>
#include <string.h>
#include <sys/mman.h>

#include "ldcs_audit_server_numa.h"
#include "ldcs_audit_server_process.h"
#include "spindle_debug.h"
#include "spindle_launch.h"

static int num_nodes;

static int initialize_numa_lib() {
   static int initialized = 0;
   int result;
   if (initialized != 0)
      return initialized;
   result = numa_available();
   if (result == -1) {
      err_printf("numa_available returned -1, disabling numa replication\n");
      initialized = -1;
      return initialized;
   }
   num_nodes = numa_num_task_nodes();
   initialized = 1;
   return initialized;
}

int numa_should_replicate(ldcs_process_data_t *procdata, char *filename)
{
   char *start, *end;
   int str_len;
   char buffer[MAX_PATH_LEN+1];
   
   if (!(procdata->opts & OPT_NUMA)) {
      return 0;
   }
   if (initialize_numa_lib() == -1) {
      return 0;
   }

   if (!procdata->numa_substrs || procdata->numa_substrs[0] == '\0') {
      debug_printf3("Numa replicating %s because NUMA replicate-all is on\n", filename);
      return 1;
   }


   start = procdata->numa_substrs;
   while (*start != '\0') {
      while (*start == ':') {
         start++;
         if (*start == '\0')
            return 0;
      }
      end = start;
      while (*end != ':' && *end != '\0')
         end++;
      str_len = end - start;
      if (str_len == 0) {
         start++;
         continue;
      }
      if (str_len > sizeof(buffer)) {
         err_printf("Numa substring component is longer than MAX_PATH_LEN.\n");
         return 0;
      }
      strncpy(buffer, start, str_len);
      buffer[str_len] = '\0';
      if (strstr(filename, buffer)) {
         debug_printf3("Numa file %s matches substring %s, replicating.\n", filename, buffer);
         return 1;
      }
      start = end;
   }
   return 0;
}

int numa_num_nodes()
{
   return num_nodes;
}

int numa_node_for_core(int core)
{
   int result = numa_node_of_cpu(core);
   if (result == -1) {
      err_printf("numa_node_of_cpu(%d) returned -1\n", core);
      return -1;
   }

   return result;
}

int numa_assign_memory_to_node(void *memory, size_t memory_size, int node)
{
   unsigned long nodemask;
   int result;

   nodemask = (1 << node);
   result = mbind(memory, memory_size, MPOL_BIND, &nodemask, num_nodes+1, 0);
   if (result == -1) {
      err_printf("numa mbind(%p, %lu, MPOL_BIND, &(%lu), %d, 0) returned -1\n",
                 memory, memory_size, nodemask, num_nodes+1);
      return -1;
   }
   return 0;
}

void *numa_alloc_temporary_memory(size_t size)
{
   void *result;
   result = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
   if (result == MAP_FAILED) {
      err_printf("Could not mmap anonymous region of size %lu\n", size);
      return NULL;
   }
   return result;
}

void numa_free_temporary_memory(void *alloc, size_t size)
{
   int result;
   result = munmap(alloc, size);
   if (result == -1) {
      err_printf("Failed to free temporary storage with munmap\n");
   }
}

void numa_update_local_filename(char *localfilename, int node)
{
   char node_str[8];
   char *numa_part, *replace_part;
   int i;
   snprintf(node_str, sizeof(node_str), "%06d", node);
   numa_part = strstr(localfilename, "spindlens-numafile-XXXXXX");
   if (!numa_part) {
      err_printf("Failed to update localfilename %s with numa replication name, because it wasn't a numa name", localfilename);
      return;
   }
   replace_part = numa_part;
   while (*replace_part != 'X')
      replace_part++;
   for (i = 0; i < 6; i++) {
      replace_part[i] = node_str[i];
   }
}

#else

int numa_should_replicate(ldcs_process_data_t *procdata, char *filename)
{
   return 0;
}

int numa_num_nodes()
{
   return 1;
}

int numa_node_for_core(int core)
{
   return 0;
}

int numa_assign_memory_to_node(void *memory, size_t memory_size, int node)
{
   return 0;
}

void *numa_alloc_temporary_memory(size_t size)
{
   return NULL;
}

void numa_free_temporary_memory(void *alloc, size_t size)
{
   return NULL;
}

void numa_update_local_filename(char *localfilename, int node)
{
}

#endif
