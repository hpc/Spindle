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

#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "spindle_debug.h"
#include "ldcs_api.h"

#define MAX_CNS_PER_ION 256

typedef struct socket_hash_t {
   uint32_t max_rank;
   uint32_t num_ranks;
   uint32_t hash_val;
   char *socket_name;
} socket_hash_t;

typedef struct cn_info_t {
   uint32_t max_rank;
   uint32_t num_ranks;
} cn_info_t;

static int num_cns;
static cn_info_t *cns = NULL;

static uint32_t *rank_table;
static unsigned int max_ranks_per_cn;
static unsigned int max_rank;

#define RANK(X, Y) rank_table[X * max_ranks_per_cn + Y]

static int write_rank_file(const char *tmpdir, socket_hash_t *socket_hash, unsigned int socket_hash_size)
{
   int fd;
   unsigned int i;
   char prepath[MAX_PATH_LEN+1];
   char path[MAX_PATH_LEN+1];
   ssize_t result;

   /* Open rankFile.pre */
   snprintf(prepath, MAX_PATH_LEN, "%s/rankFile.pre", tmpdir);
   prepath[MAX_PATH_LEN] = '\0';
   fd = open(prepath, O_CREAT|O_EXCL|O_WRONLY, 0600);
   if (fd == -1) {
      err_printf("Unable to create rankfile %s: %s\n", prepath, strerror(errno));
      return -1;
   }

   /* Write the rank counts to rankFile.pre */
   for (i = 0; i < socket_hash_size; i++) {
      if (!socket_hash[i].socket_name)
         continue;
      result = write(fd, & socket_hash[i].max_rank, sizeof(socket_hash[i].max_rank));
      if (result == -1) {
         err_printf("Unable to write max_rank to %s: %s\n", prepath, strerror(errno));
         return -1;
      }
      result = write(fd, & socket_hash[i].num_ranks, sizeof(socket_hash[i].num_ranks));
      if (result == -1) {
         err_printf("Unable to write num_ranks to %s: %s\n", prepath, strerror(errno));
         return -1;
      }
   }
   close(fd);

   /* Move rankFile.pre to rankFile */
   snprintf(path, MAX_PATH_LEN, "%s/rankFile", tmpdir);
   prepath[MAX_PATH_LEN] = '\0';   
   result = rename(prepath, path);
   if (result == -1) {
      err_printf("Unable to rename %s to %s\n", prepath, path);
      return -1;
   }

   return 0;
}

static unsigned int str_hash(const char *str)
{
   unsigned long hash = 5381;
   int c;
   while ((c = *str++))
      hash = ((hash << 5) + hash) + c;
   return hash;
}

static socket_hash_t *find_or_add_in_table(const char *socket_name, socket_hash_t *table)
{
   unsigned int hash_val;
   int i, start_val;

   hash_val = str_hash(socket_name);
   i = start_val = (hash_val % MAX_CNS_PER_ION);

   for (;;) {
      if (hash_val == table[i].hash_val && strcmp(socket_name, table[i].socket_name) == 0)
         return table + i;
      if (table[i].socket_name == NULL) {
         table[i].socket_name = strdup(socket_name);
         table[i].hash_val = hash_val;
         table[i].max_rank = table[i].num_ranks = 0;
         num_cns++;
         return table + i;
      }
      
      i++;
      if (i == MAX_CNS_PER_ION)
         i = 0;
      assert(i != start_val);
   }
}

int biterd_init_comms(const char *tmpdir)
{
   static int ret_val = -1;
   int jobid, rank, i, j;
   char *jobid_s, *fname;
   char path[128], socket_path[128];
   DIR *dir;
   struct dirent *de;
   ssize_t result;
   socket_hash_t *entry;
   socket_hash_t socket_hash[MAX_CNS_PER_ION];

   if (cns)
      return ret_val;
   
   jobid_s = getenv("BG_JOBID");
   if (jobid_s == NULL)
      return -1;
   jobid = atoi(jobid_s);
   
   snprintf(path, sizeof(path), "/jobs/%d/toolctl_rank", jobid);
   dir = opendir(path);
   if (dir == NULL)
      return -1;

   memset(socket_hash, 0, sizeof(socket_hash));
   while ((de = readdir(dir)) != NULL) {
      fname = de->d_name;
      if (fname[0] == '\0' || fname[0] == '.')
         continue;
      rank = atoi(fname);

      snprintf(path, sizeof(path), "/jobs/%d/toolctl_rank/%d", jobid, rank);
      
      memset(socket_path, 0, sizeof(socket_path));
      result = readlink(path, socket_path, sizeof(socket_path));
      socket_path[sizeof(socket_path)-1] = '\0';
      if (result == -1) {
         closedir(dir);
         return -1;
      }

      entry = find_or_add_in_table(socket_path, socket_hash);
      assert(entry);

      if (rank > entry->max_rank)
         entry->max_rank = rank;
      if (rank > max_rank)
         max_rank = rank;
      entry->num_ranks++;
      if (entry->num_ranks > max_ranks_per_cn)
         max_ranks_per_cn = entry->num_ranks;
   }

   closedir(dir);

   result = write_rank_file(tmpdir, socket_hash, MAX_CNS_PER_ION);
   if (result == -1)
      return -1;

   cns = malloc(sizeof(cn_info_t) * num_cns);
   for (i = 0, j = 0; i < MAX_CNS_PER_ION; i++) {
      if (socket_hash[i].socket_name == NULL)
         continue;

      assert(j < num_cns);
      cns[j].max_rank = socket_hash[i].max_rank;
      cns[j].num_ranks = socket_hash[i].num_ranks;
      j++;
      free(socket_hash[i].socket_name);
   }

   rank_table = malloc(sizeof(rank_table[0]) * max_ranks_per_cn * num_cns);
   assert(rank_table);

   ret_val = 0;
   return ret_val;
}

int biterd_num_compute_nodes()
{
   return num_cns;
}

int biterd_ranks_in_cn(int cn_id)
{
   assert(cn_id < num_cns);
   return cns[cn_id].num_ranks;
}

int biterd_unique_num_for_cn(int cn_id)
{
   assert(cn_id < num_cns);
   return cns[cn_id].max_rank;
}

int biterd_register_rank(int session_id, uint32_t client_id, uint32_t rank)
{
   assert(session_id >= 0 && session_id < num_cns);
   assert(client_id < cns[session_id].num_ranks);
   assert(rank <= cns[session_id].max_rank);
   assert(rank <= max_rank);
   assert(rank_table);
   
   RANK(session_id, client_id) = rank;
   return 0;
}

int biterd_get_rank(int compute_node_id, int client_id)
{
   return RANK(compute_node_id, client_id);
}

