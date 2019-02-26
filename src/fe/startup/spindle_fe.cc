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

#include "parse_preload.h"
#include "ldcs_api.h"
#include "spindle_launch.h"
#include "fe_comm.h"
#include "parseargs.h"
#include "keyfile.h"
#include "config.h"
#include "spindle_debug.h"
#include "ldcs_cobo.h"

#include <string>
#include <cassert>
#include <pwd.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>

#if defined(USAGE_LOGGING_FILE)
static const char *logging_file = USAGE_LOGGING_FILE;
#else
static const char *logging_file = NULL;
#endif
static const char spindle_bootstrap[] = LIBEXECDIR "/spindle_bootstrap";

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
   buffer_size = sizeof(unsigned int) * 8;
   buffer_size += sizeof(opt_t);
   buffer_size += sizeof(unique_id_t);
   buffer_size += args->location ? strlen(args->location) + 1 : 1;
   buffer_size += args->pythonprefix ? strlen(args->pythonprefix) + 1 : 1;
   buffer_size += args->preloadfile ? strlen(args->preloadfile) + 1 : 1;

   unsigned int pos = 0;
   char *buf = (char *) malloc(buffer_size);
   pack_param(args->number, buf, pos);
   pack_param(args->port, buf, pos);
   pack_param(args->num_ports, buf, pos);
   pack_param(args->opts, buf, pos);
   pack_param(args->unique_id, buf, pos);
   pack_param(args->use_launcher, buf, pos);
   pack_param(args->startup_type, buf, pos);
   pack_param(args->shm_cache_size, buf, pos);
   pack_param(args->location, buf, pos);
   pack_param(args->pythonprefix, buf, pos);
   pack_param(args->preloadfile, buf, pos);
   pack_param(args->bundle_timeout_ms, buf, pos);
   pack_param(args->bundle_cachesize_kb, buf, pos);
   assert(pos == buffer_size);

   buffer = (void *) buf;
   return 0;
}

static void logUser()
{
   /* Collect username */
   char *username = NULL;
   if (getenv("USER")) {
      username = getenv("USER");
   }
   if (!username) {
      struct passwd *pw = getpwuid(getuid());
      if (pw) {
         username = pw->pw_name;
      }
   }
   if (!username) {
      username = getlogin();
   }
   if (!username) {
      err_printf("Could not get username for logging\n");
   }
      
   /* Collect time */
   struct timeval tv;
   gettimeofday(&tv, NULL);
   struct tm *lt = localtime(& tv.tv_sec);
   char time_str[256];
   time_str[0] = '\0';
   strftime(time_str, sizeof(time_str), "%c", lt);
   time_str[255] = '\0';
   
   /* Collect version */
   const char *version = VERSION;

   /* Collect hostname */
   char hostname[256];
   hostname[0] = '\0';
   gethostname(hostname, sizeof(hostname));
   hostname[255] = '\0';

   string log_message = string(username) + " ran Spindle v" + version + 
                        " at " + time_str + " on " + hostname;
   debug_printf("Logging usage: %s\n", log_message.c_str());

   FILE *f = fopen(logging_file, "a");
   if (!f) {
      err_printf("Could not open logging file %s\n", logging_file);
      return;
   }
   fprintf(f, "%s\n", log_message.c_str());
   fclose(f);
}

static void setupSecurity(spindle_args_t *params)
{
   handshake_protocol_t handshake;
   int result;
   /* Setup security */
   switch (OPT_GET_SEC(params->opts)) {
      case OPT_SEC_MUNGE:
         debug_printf("Initializing FE with munge-based security\n");
         handshake.mechanism = hs_munge;
         break;
      case OPT_SEC_KEYFILE: {
         char *path;
         int len;
         debug_printf("Initializing FE with keyfile-based security\n");
         create_keyfile(params->unique_id);
         len = MAX_PATH_LEN+1;
         path = (char *) malloc(len);
         get_keyfile_path(path, len, params->unique_id);
         handshake.mechanism = hs_key_in_file;
         handshake.data.key_in_file.key_filepath = path;
         handshake.data.key_in_file.key_length_bytes = KEY_SIZE_BYTES;
         break;
      }
      case OPT_SEC_KEYLMON:
         debug_printf("Initializing BE with launchmon-based security\n");
         handshake.mechanism = hs_explicit_key;
         fprintf(stderr, "Error, launchmon based keys not yet implemented\n");
         exit(-1);
         break;
      case OPT_SEC_NULL:
         debug_printf("Initializing BE with NULL security\n");
         handshake.mechanism = hs_none;
         break;
   }
   result = initialize_handshake_security(&handshake);
   if (result == -1) {
      err_printf("Could not initialize security\n");
      exit(-1);
   }
}

int getApplicationArgsFE(spindle_args_t *params, int *spindle_argc, char ***spindle_argv)
{
   char number_s[32];
   char opt_s[32];
   char cachesize_s[32];

   snprintf(number_s, sizeof(number_s), "%u", params->number);
   snprintf(opt_s, sizeof(opt_s), "%lu", (unsigned long) params->opts);
   snprintf(cachesize_s, sizeof(cachesize_s), "%u", params->shm_cache_size);
   
   *spindle_argv = (char **) malloc(sizeof(char*) * 6);
   (*spindle_argv)[0] = strdup(spindle_bootstrap);
   (*spindle_argv)[1] = strdup(params->location);
   (*spindle_argv)[2] = strdup(number_s);
   (*spindle_argv)[3] = strdup(opt_s);
   (*spindle_argv)[4] = strdup(cachesize_s);
   (*spindle_argv)[5] = NULL;
   *spindle_argc = 5;

   return 0;
}

void fillInSpindleArgsFE(spindle_args_t *params)
{
   LOGGING_INIT(const_cast<char *>("FE"));

   //parseCommandLine will fill in the params.  Call it 
   // with a fake, empty command line to get defaults.
   char *fake_argv[3];
   fake_argv[0] = const_cast<char *>("spindle");
   fake_argv[1] = const_cast<char *>("launcher");
   fake_argv[2] = NULL;
   parseCommandLine(2, fake_argv, params);

   params->use_launcher = external_launcher;
   params->startup_type = startup_external;
}

static void *md_data_ptr;

int spindleInitFE(const char **hosts, spindle_args_t *params)
{
   debug_printf("Called spindleInitFE\n");

   if (params->opts & OPT_LOGUSAGE)
      logUser();

   setupSecurity(params);

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
                                params->port, params->num_ports, params->unique_id,
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

int spindleCloseFE(spindle_args_t *params)
{
   if (OPT_GET_SEC(params->opts) == OPT_SEC_KEYFILE) {
      clean_keyfile(params->unique_id);
   }

   ldcs_audit_server_fe_md_close(md_data_ptr);

   if (params->startup_type == startup_external)
      LOGGING_FINI;

   return 0;
}
