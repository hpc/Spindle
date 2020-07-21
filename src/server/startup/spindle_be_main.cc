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

#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include "spindle_debug.h"
#include "spindle_launch.h"
#include "ldcs_api.h"
#include "config.h"
#include "cleanup_proc.h"

static void setupLogging();
static int parseCommandLine(int argc, char *argv[]);

#if defined(HAVE_LMON)
extern int startLaunchmonBE(int argc, char *argv[], int security_type);
#endif
extern int startHostbinBE(unsigned int port, unsigned int num_ports, unique_id_t unique_id, int security_type);
extern int startSerialBE(int argc, char *argv[], int security_type);
extern int startMPILaunchBE(unsigned int port, unsigned int num_ports, unique_id_t unique_id, int security_type);

enum startup_type_t {
   lmon,
   serial,
   hostbin,
   mpilaunch,
   selflaunch
};
startup_type_t startup_type;
static int security_type;
static int number;
static int port;
static int num_ports;
static unique_id_t unique_id;

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

   switch (startup_type) {
      case lmon:
#if defined(HAVE_LMON)
         result = startLaunchmonBE(argc, argv, security_type);
#else
         assert(0);
#endif
         break;
      case serial:
         result = startSerialBE(argc, argv, security_type);
         break;
      case hostbin:
         result = startHostbinBE(port, num_ports, unique_id, security_type);
         break;
      case mpilaunch:
         result = startMPILaunchBE(port, num_ports, unique_id, security_type);
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
      else if (strcmp(argv[i], "--spindle_hostbin") == 0) {
         startup_type = hostbin;
         break;
      }
      else if (strcmp(argv[i], "--spindle_mpi") == 0) {
         startup_type = mpilaunch;
         break;
      }
      else if (strcmp(argv[i], "--spindle_selflaunch") == 0) {
         startup_type = selflaunch;
         break;
      }
   }

   if (++i >= argc) return -1;   
   security_type = atoi(argv[i]);

   if (++i >= argc) return -1;   
   number = atoi(argv[i]);

   if (startup_type == hostbin || startup_type == mpilaunch || startup_type == selflaunch) {
      if (++i >= argc) return -1;
      port = atoi(argv[i]);
      if (++i >= argc) return -1;
      num_ports = atoi(argv[i]);
      if (++i >= argc) return -1;
      unique_id = strtoul(argv[i], NULL, 10);
   }

   return 0;
}
