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

#include "parse_preload.h"
#include "ldcs_api.h"
#include "spindle_launch.h"
#include "fe_comm.h"

#include <string>
#include <cassert>

#include <stdlib.h>
#include <stdint.h>

using namespace std;

template<typename T>
void pack_param(T value, char *buffer, unsigned int &pos)
{
   memcpy(buffer + pos, &value, sizeof(T));
   pos += sizeof(T);
}

template<>
void pack_param<char*>(char *value, char *buffer, unsigned int &pos)
{
   if (value == NULL) {
      value = const_cast<char*>("");
   }
   unsigned int strsize = strlen(value) + 1;
   memcpy(buffer + pos, value, strsize);
   pos += strsize;
}

static int pack_data(spindle_args_t *args, void* &buffer, unsigned &buffer_size)
{  
   buffer_size = sizeof(unsigned int) * 5;
   buffer_size += args->location ? strlen(args->location) + 1 : 1;
   buffer_size += args->pythonprefix ? strlen(args->pythonprefix) + 1 : 1;
   buffer_size += args->preloadfile ? strlen(args->preloadfile) + 1 : 1;

   unsigned int pos = 0;
   char *buf = (char *) malloc(buffer_size);
   pack_param(args->number, buf, pos);
   pack_param(args->port, buf, pos);
   pack_param(args->opts, buf, pos);
   pack_param(args->shared_secret, buf, pos);
   pack_param(args->use_launcher, buf, pos);
   pack_param(args->location, buf, pos);
   pack_param(args->pythonprefix, buf, pos);
   pack_param(args->preloadfile, buf, pos);
   assert(pos == buffer_size);

   buffer = (void *) buf;
   return 0;
}

static void *md_data_ptr;

int spindleInitFE(const char **hosts, spindle_args_t *params)
{
   debug_printf("Called spindleInitFE\n");

   /* Create preload message before initializing network to detect errors */
   ldcs_message_t *preload_msg = NULL;
   if (params->opts & OPT_PRELOAD) {
      string preload_file = string(params->preloadfile);
      preload_msg = parsePreloadFile(preload_file);
      if (!preload_msg) {
         fprintf(stderr, "Failed to parse preload file %s\n", preload_file.c_str());
         return -1;
      }
   }

   /* Compute hosts size */
   unsigned int hosts_size = 0;
   for (const char **h = hosts; *h != NULL; h++, hosts_size++);

   /* Start FE server */
   debug_printf("Starting FE servers with hostlist of size %u on port %u\n", hosts_size, params->port);
   ldcs_audit_server_fe_md_open(const_cast<char **>(hosts), hosts_size, 
                                params->port, params->shared_secret,
                                &md_data_ptr);

   /* Broadcast parameters */
   debug_printf("Sending parameters to servers\n");
   void *param_buffer;
   unsigned int param_buffer_size;
   pack_data(params, param_buffer, param_buffer_size);
   ldcs_message_t msg;
   msg.header.type = LDCS_MSG_SETTINGS;
   msg.header.len = param_buffer_size;
   msg.data = static_cast<char *>(param_buffer);
   ldcs_audit_server_fe_broadcast(&msg, md_data_ptr);
   free(param_buffer);

   /* Broadcast preload contents */
   if (preload_msg) {
      debug_printf("Sending message with preload information\n");
      ldcs_audit_server_fe_broadcast(preload_msg, md_data_ptr);
      cleanPreloadMsg(preload_msg);
   }

   return 0;   
}

int spindleCloseFE()
{
   ldcs_audit_server_fe_md_close(md_data_ptr);
   return 0;
}
