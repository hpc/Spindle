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

#include <unistd.h>
#include <stdlib.h>
#include "spindle_debug.h"
#include "spindle_launch.h"
#include "keyfile.h"
#include "handshake.h"
#include "ldcs_api.h"
#include "ldcs_cobo.h"
#include "config.h"

static void setupLogging();
static int parseCommandLine(int argc, char *argv[]);
static void initSecurity();

extern int startLaunchmonBE(int argc, char *argv[]);
extern int startSerialBE(int argc, char *argv[]);
enum startup_type_t {
   lmon,
   serial
};
startup_type_t startup_type;
static int security_type;
static int number;

int main(int argc, char *argv[])
{
   int  result;
   setupLogging();

   debug_printf("Spindle Server Cmdline: ");
   for (int i=0; i<argc; i++) {
      bare_printf("%s ", argv[i]);
   }
   bare_printf("\n");

   result = parseCommandLine(argc, argv);
   if (result == -1) {
      err_printf("Could not parse command line\n");
      fprintf(stderr, "Error, spindle_be must be launched via spindle\n");
      exit(-1);
   }

   initSecurity();

   switch (startup_type) {
      case lmon:
         result = startLaunchmonBE(argc, argv);
         break;
      case serial:
         result = startSerialBE(argc, argv);
         break;
      default:
         err_printf("Unknown startup mode\n");
         result = -1;
         break;
   }

   LOGGING_FINI;
   return result;
}

static void setupLogging()
{
   LOGGING_INIT(const_cast<char *>("Server"));
   if (!spindle_debug_output_f)
      return;

   int fd = fileno(spindle_debug_output_f);
   if (fd == -1)
      return;

   close(1);
   close(2);
   dup2(fd, 1);
   dup2(fd, 2);
}

static int parseCommandLine(int argc, char *argv[])
{
   int i;
   for (i = 0; i < argc; i++) {
      if (strcmp(argv[i], "--spindle_lmon") == 0) {
         startup_type = lmon;
         break;
      }
      else if (strcmp(argv[i], "--spindle_serial") == 0) {
         startup_type = serial;
         break;
      }
   }

   i++;
   if (i >= argc) return -1;   
   security_type = atoi(argv[i]);

   i++;
   if (i >= argc) return -1;   
   number = atoi(argv[i]);

   return 0;
}

static void initSecurity()
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
         get_keyfile_path(path, len, number);
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
