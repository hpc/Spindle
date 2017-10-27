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

#include <stdint.h>

#if !defined(SPINDLE_LAUNCH_H_)
#define SPINDLE_LAUNCH_H_

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(SPINDLEFE) || defined(SPINDLEBE) || defined(SPINDLE_DO_EXPORT)
#define SPINDLE_EXPORT __attribute__((visibility ("default")))
#else
#define SPINDLE_EXPORT
#endif

/* Bitfield values for opts parameter */   /* Bit is set if ... */
#define OPT_COBO       (1 << 1)             /* COBO is the communication implementation */
#define OPT_DEBUG      (1 << 2)             /* Hide Spindle from debuggers (currently unnecessary) */
#define OPT_FOLLOWFORK (1 << 3)             /* Spindle should follow and manage child processes */
#define OPT_PRELOAD    (1 << 4)             /* Spindle should pre-stage libraries from a preload file */
#define OPT_PUSH       (1 << 5)             /* Libraries are staged to all nodes when one node requests */
#define OPT_PULL       (1 << 6)             /* Libraries are staged to nodes that request them */
#define OPT_RELOCAOUT  (1 << 7)             /* Relocate initial executable */
#define OPT_RELOCSO    (1 << 8)             /* Relocate libraries */
#define OPT_RELOCEXEC  (1 << 9)             /* Relocate the targets of exec() calls */
#define OPT_RELOCPY    (1 << 10)            /* Relocate python .py/.pyc/.pyo files */
#define OPT_STRIP      (1 << 11)            /* Strip debug information from ELF files */
#define OPT_NOCLEAN    (1 << 12)            /* Don't clean stage area on exit (useful for debugging) */
#define OPT_NOHIDE     (1 << 13)            /* Hide Spindle's communication FDs from application */
#define OPT_REMAPEXEC  (1 << 14)            /* Use remapping hack to make /proc/PID/exe point to original exe */
#define OPT_LOGUSAGE   (1 << 15)            /* Log usage information to a file */
#define OPT_SHMCACHE   (1 << 16)            /* Use a shared memory cache optimization (only needed on BlueGene) */
#define OPT_SUBAUDIT   (1 << 17)            /* Use subaudit mechanism (needed on BlueGene and very old GLIBCs) */
#define OPT_PERSIST    (1 << 18)            /* Spindle servers should not exit when all clients exit. */
#define OPT_SEC        (7 << 19)            /* Security mode, one of the below OPT_SEC_* values */
#define OPT_SESSION    (1 << 22)            /* Session mode, where Spindle lifetime spans jobs */

#define OPT_SET_SEC(OPT, X) OPT |= (X << 19)
#define OPT_GET_SEC(OPT) ((OPT >> 19) & 7)
#define OPT_SEC_MUNGE 0                     /* Use munge to validate connections */
#define OPT_SEC_KEYLMON 1                   /* Use LaunchMON transmitted keys to validate */
#define OPT_SEC_KEYFILE 2                   /* Use a key from a shared file to validate */
#define OPT_SEC_NULL 3                      /* Do not validate connections */

/* Possible values for use_launcher, describe how the job is started */
#define srun_launcher (1 << 0)              /* Job is launched via SLURM */
#define serial_launcher (1 << 1)            /* Job is a non-parallel app launched via fork/exec */
#define openmpi_launcher (1 << 2)           /* Job is launched via ORTE */
#define wreckrun_launcher (1 << 3)          /* Job is launched via FLUX's job launcher */
#define marker_launcher (1 << 4)            /* Unknown job launcher with Spindle markers in launch line */
#define external_launcher (1 << 5)          /* An external mechanism starts application */
#define unknown_launcher (1 << 5)           /* Deprecated alias for external launcher */

/* Possible values for startup_type, describe how Spindle servers are started */
#define startup_serial 0                    /* Job is non-parallel app, and server is forked/exec */
#define startup_lmon 1                      /* Start via LaunchMON */
#define startup_hostbin 2                   /* Start via hostbin */
#define startup_external 3                  /* An external mechanism starts Spindle */
#define startup_mpi 4                       /* Start via the MPI launcher */

typedef uint64_t unique_id_t;
typedef uint64_t opt_t;

/* Parameters for configuring Spindle */
typedef struct {
   /* A unique number that will be used to identify this spindle session */
   unsigned int number;

   /* The beginning port in a range that will be used for server->server communication */
   unsigned int port;

   /* The number of ports in the port range */
   unsigned int num_ports;

   /* A bitfield of the above OPT_* values */
   opt_t opts;

   /* A unique number that all servers will need
      to provide to join the Spindle network */
   unique_id_t unique_id;

   /* The type of the MPI launcher */
   unsigned int use_launcher;

   /* The mechanism used to start Spindle daemons */
   unsigned int startup_type;

   /* Size of client shared memory cache */
   unsigned int shm_cache_size;

   /* The local-disk location where Spindle will store its cache */
   char *location;

   /* Colon-seperated list of directories where Python is installed */
   char *pythonprefix;

   /* Name of a white-space delimited file containing a list of files that will be preloaded */
   char *preloadfile;
} spindle_args_t;

/* Functions used to startup Spindle on the front-end. Init returns after finishing start-up,
   and it is the caller's responsibility to call spindleCloseFE when the servers terminate.
   spindleInitFE should be called with a filled in params and after the applications
   and daemons have been launched.*/
SPINDLE_EXPORT int spindleInitFE(const char **hosts, spindle_args_t *params);
SPINDLE_EXPORT int spindleCloseFE(spindle_args_t *params);

/* Fill in a spindle_args_t with default values */
SPINDLE_EXPORT void fillInSpindleArgsFE(spindle_args_t *params);

/* Given a filled in spindle_args_t, getApplicationArgsFE gets the
   array of arguments that should be inserted into the application
   launch line.  The new arguments should go before the application
   executable.

   Upon completion, *spindle_argv will be set to point at a malloc'd
   array of strings (each of which is also malloc'd), that should be
   inserted.  *spindle_argc will be set to the size of the
   *spindle_argv array.  spindle_argv and its strings can be free'd at
   any time.

   For example, the line
     mpirun -n 512 mpi_app
   Should be changed to
     mpirun -n 512 SPINDLE_ARGV[] mpi_app 
*/
SPINDLE_EXPORT int getApplicationArgsFE(spindle_args_t *params, int *spindle_argc, char ***spindle_argv);

/* Runs the server process on a BE, returns when server is done */
SPINDLE_EXPORT int spindleRunBE(unsigned int port, unsigned int num_ports, unique_id_t unique_id, int security_type,
                                int (*post_setup)(spindle_args_t *));

/* Bitmask of values for the test_launchers parameter */
#define TEST_PRESETUP 1<<0
#define TEST_SERIAL   1<<1
#define TEST_SLURM    1<<2
#define TEST_FLUX     1<<3

#if defined(__cplusplus)
}
#endif

#endif

