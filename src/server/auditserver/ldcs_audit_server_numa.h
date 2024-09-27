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

#if !defined LDCS_AUDIT_SERVER_NUMA_H_
#define LDCS_AUDIT_SERVER_NUMA_H_

#include "ldcs_audit_server_process.h"

int numa_should_replicate(ldcs_process_data_t *procdata, char *filename);
int numa_num_nodes();
int numa_node_for_core(int core);
int numa_assign_memory_to_node(void *memory, size_t memory_size, int node);
void *numa_alloc_temporary_memory(size_t size);
void numa_free_temporary_memory(void *alloc, size_t size);
void numa_update_local_filename(char *localfilename, int node);

#endif

