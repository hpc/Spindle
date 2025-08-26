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
#include "ldcs_audit_server_process.h"
#include "parseloc.h"
#include "ldcs_audit_server_md.h"
#include "ldcs_api.h"
#include "keyfile.h"
#include "handshake.h"
#include "ldcs_cobo.h"
#include "exitnote.h"

#include <cstdlib>
#include <cassert>
#include <unistd.h>

extern int spindle_mkdir(char *orig_path);
extern int releaseApplication();


#if defined(__cplusplus)
extern "C" {
#endif

extern void getFirstValidPath( char **realizedPath, char **parsedPath, char **symbolicPath, char *origPathList, number_t number );
extern void determineValidCachePaths( uint64_t *validBitIdx, char *origPathList, number_t number );
extern void getValidCachePathByIndex( uint64_t validBitIdx, char **realizedCachePath, char **parsedCachePath, char **symbolicCachePath );

#if defined(__cplusplus)
}
#endif

template<typename T>
void unpack_param(T &value, char *buffer, int &pos)
{
   memcpy(&value, buffer + pos, sizeof(T));
   pos += sizeof(T);
}

template<>
void unpack_param<char*>(char* &value, char *buffer, int &pos)
{
   unsigned int strsize = strlen(buffer + pos) + 1;
   value = (char *) malloc(strsize);
   strncpy(value, buffer + pos, strsize);
   pos += strsize;
}

static int unpack_data(spindle_args_t *args, void *buffer, int buffer_size)
{
   int pos = 0;
   char *buf = static_cast<char *>(buffer);
   unpack_param(args->number, buf, pos);
   unpack_param(args->port, buf, pos);
   unpack_param(args->num_ports, buf, pos);
   unpack_param(args->opts, buf, pos);
   unpack_param(args->unique_id, buf, pos);
   unpack_param(args->use_launcher, buf, pos);
   unpack_param(args->startup_type, buf, pos);
   unpack_param(args->shm_cache_size, buf, pos);
   unpack_param(args->candidate_locations, buf, pos);
   unpack_param(args->candidate_cachepaths, buf, pos);
   unpack_param(args->candidate_commpaths, buf, pos);
   unpack_param(args->pythonprefix, buf, pos);
   unpack_param(args->preloadfile, buf, pos);
   unpack_param(args->bundle_timeout_ms, buf, pos);
   unpack_param(args->bundle_cachesize_kb, buf, pos);
   unpack_param(args->numa_files, buf, pos);
   unpack_param(args->numa_excludes, buf, pos);
   unpack_param(args->rsh_command, buf, pos);
   unpack_param(args->local_prefixes, buf, pos);
   unpack_param(args->session_key, buf, pos);
   unpack_param(args->exec_excludes, buf, pos);
   assert(pos == buffer_size);

   return 0;    
}

static void initSecurity(int security_type, uint64_t unique_id)
{
   int result;
   handshake_protocol_t handshake;
   switch (security_type) {
      case OPT_SEC_MUNGE:
         debug_printf("Initializing BE with munge-based security\n");
         handshake.mechanism = hs_munge;
         break;
      case OPT_SEC_KEYFILE: {
         char *path;
         int len;
         debug_printf("Initializing BE with keyfile-based security\n");
         len = MAX_PATH_LEN+1;
         path = (char *) malloc(len);
         get_keyfile_path(path, len, unique_id);
         handshake.mechanism = hs_key_in_file;
         handshake.data.key_in_file.key_filepath = path;
         handshake.data.key_in_file.key_length_bytes = KEY_SIZE_BYTES;
         break;
      }
      case OPT_SEC_KEYLMON: {
         debug_printf("Initializing BE with launchmon-based security\n");
         handshake.mechanism = hs_explicit_key;
         err_printf("Error, launchmon based keys not yet implemented\n");
         exit(-1);
         break;
      }
      case OPT_SEC_NULL:
         handshake.mechanism = hs_none;
         debug_printf("Initializing BE with NULL security\n");
         break;
   }

   result = initialize_handshake_security(&handshake);
   if (result == -1) {
      err_printf("Could not initialize security\n");
      exit(-1);
   }
}

int spindleRunBE(unsigned int port, unsigned int num_ports, unique_id_t unique_id, int security_type,
                 int (*post_setup)(spindle_args_t *))
{
   int result;
   spindle_args_t args;

   LOGGING_INIT(const_cast<char *>("Server"));

   initSecurity(security_type, unique_id);

   debug_printf("Called spindleRunBE(port=%u, num_ports=%u, unique_id = %lu, security_type = %d, post_setup = %p)\n",
                port, num_ports, unique_id, security_type, post_setup);
   
   /* Setup network and share setup data */
   debug_printf3("spindleRunBE setting up network and receiving setup data\n");
   void *setup_data;
   int setup_data_size;
   result = ldcs_audit_server_network_setup(port, num_ports, unique_id, &setup_data, &setup_data_size);
   if (result == -1) {
      err_printf("Error setting up network in spindleRunBE\n");
      return -1;
   }
   unpack_data(&args, setup_data, setup_data_size);
   free(setup_data);
   assert(args.unique_id == unique_id);
   assert(args.port == port);
   
   
   // For each of args.location, args.cachepaths, and args.commpaths, parse each
   // candidate directory in the list (replacing environment variables with their
   // values) and attempt to create that directory.  On success, replace the list
   // of paths with the path to the created directory.

   getFirstValidPath( &args.chosen_location,  NULL,  NULL,  args.candidate_locations, args.number );
   if( NULL == args.chosen_location ){
       err_printf("No valid location path available.\n");
       return -1;
   }

   getFirstValidPath( &args.chosen_commpath,  NULL,  NULL,  args.candidate_commpaths, args.number );
   if( NULL == args.chosen_commpath ){
       err_printf("No valid cachepath path available.  Falling back to \"location\" path (%s).\n", args.chosen_location);
       args.chosen_commpath = args.chosen_location;
   }

   determineValidCachePaths( &args.cachepath_bitidx, args.candidate_cachepaths, args.number);
   /* QQQ FIXME Eventually this call will be moved to the LDCS_MSG_LOCATION_CONSENSUS message handler. */
   getValidCachePathByIndex( args.cachepath_bitidx, &args.chosen_cachepath, NULL, NULL );
   if( NULL == args.chosen_cachepath ){
       err_printf("No valid cachepath path available.  Falling back to \"location\" path (%s).\n", args.chosen_location);
       args.chosen_cachepath = args.chosen_location;
   }
   debug_printf( "QQQ location=%s, commpath=%s, cachepath=%s\n",
           args.chosen_location, args.chosen_commpath, args.chosen_cachepath );

   // The test verifier is isolated behind the logger.  To set these paths, we
   // pass in "magic" logging messages.
   test_printf("<internal> location=%s\n", args.chosen_location);
   test_printf("<internal> cachepath=%s\n", args.chosen_cachepath);
   test_printf("<internal> commpath=%s\n", args.chosen_commpath); /* QQQ Move to message handler */

   result = ldcs_audit_server_process(&args);
   if (result == -1) {
      err_printf("Error in ldcs_audit_server_process\n");
      return -1;
   }

   if (post_setup) {
      result = post_setup(&args);
      if (result == -1) {
         err_printf("post_setup callback errored.  Returning\n");
         return -1;
      }
   }

   debug_printf("Setup done.  Running server.\n");
   ldcs_audit_server_run();
   if (result == -1) {
      err_printf("Error in ldcs_audit_server_process\n");
      return -1;
   }


   if (args.startup_type == startup_external)   
      LOGGING_FINI;

   return 0;
}

int spindleExitBE(const char *location)
{
   debug_printf("User called spindleExitBE(%s)\n", location);
   return pingExitNote(location);
}
