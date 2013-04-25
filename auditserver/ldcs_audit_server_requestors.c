/*
This file is part of Spindle.  For copyright information see the COPYRIGHT 
file in the top level directory, or at 
<TODO:URL>.

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

#include <stdlib.h>
#include <string.h>
#include "ldcs_audit_server_requestors.h"

struct requested_file_struct
{
   char *path;
   int hash_val;
   int requestors_num;
   int requestors_size;
   node_peer_t *requestors;
   struct requested_file_struct *next;
   struct requested_file_struct *prev;
};
typedef struct requested_file_struct requested_file_t;

#define INITIAL_PEER_SIZE 8
#define REQUESTORS_TABLE_SIZE 1024
static requested_file_t *requested_file_table[REQUESTORS_TABLE_SIZE];

static unsigned int hashval(char *str) 
{
   unsigned int hash = 5381;
   unsigned int c;
   while ((c = *str++))
      hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
   return hash % REQUESTORS_TABLE_SIZE;
}

static requested_file_t *get_requestor(char *file, int add)
{
   unsigned int val;
   requested_file_t *cur;
   
   val = hashval(file);
   for (cur = requested_file_table[val]; cur != NULL; cur = cur->next) {
      if (cur->hash_val != val || strcmp(cur->path, file) != 0) 
         continue;
      return cur;
   }
   if (!add)
      return NULL;

   cur = (requested_file_t *) malloc(sizeof(requested_file_t));
   cur->path = strdup(file);
   cur->hash_val = val;
   cur->requestors_num = 0;
   cur->requestors_size = INITIAL_PEER_SIZE;
   cur->requestors = (node_peer_t *) malloc(sizeof(node_peer_t) * INITIAL_PEER_SIZE);
   if (requested_file_table[val])
      requested_file_table[val]->prev = cur;
   cur->next = requested_file_table[val];
   cur->prev = NULL;
   
   requested_file_table[val] = cur;
   return cur;
}

int been_requested(char *file)
{
   return (get_requestor(file, 0) != NULL);
}

void add_requestor(char *file, node_peer_t peer)
{
   requested_file_t *cur = get_requestor(file, 1);
   int i;

   for (i = 0; i < cur->requestors_num; i++) {
      if (cur->requestors[i] == peer)
         return;
   }

   if (cur->requestors_num == cur->requestors_size) {
      cur->requestors_size *= 2;
      cur->requestors = realloc(cur->requestors, sizeof(node_peer_t) * cur->requestors_size);
   }
   cur->requestors[cur->requestors_num] = peer;
   cur->requestors_num++;
}

int get_requestors(char *file, node_peer_t **requestor_list, int *requestor_list_size)
{
   requested_file_t *req;
   req = get_requestor(file, 0);
   if (!req)
      return -1;
   *requestor_list = req->requestors;
   *requestor_list_size = req->requestors_num;
   return 0;
}

void clear_requestor(char *file)
{
   requested_file_t *cur = get_requestor(file, 0);
   if (!cur) return;

   if (cur->prev)
      cur->prev->next = cur->next;
   if (cur->next)
      cur->next->prev = cur->prev;
   if (requested_file_table[cur->hash_val] == cur)
      requested_file_table[cur->hash_val] = cur->next;

   free(cur->path);
   free(cur->requestors);
   free(cur);
}
