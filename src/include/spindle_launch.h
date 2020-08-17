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
#include <unistd.h>

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
#define OPT_MSGBUNDLE  (1 << 23)            /* Message bundling, which can improve high-latency network performance */
#define OPT_SELFLAUNCH (1 << 24)            /* Use a startup mode where the clients launch the daemon */
#define OPT_BEEXIT     (1 << 25)            /* Block exit until each backend calls spindleExitBE */
#define OPT_PROCCLEAN  (1 << 26)            /* Use a dedicated process to run cleanup routines after Spindle exits */
#define OPT_RSHLAUNCH  (1 << 27)            /* Launch BEs via an rsh/ssh tree */

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
#define slurm_plugin_launcher (1 << 6)      /* Launched via a SLURM spank plugin */
#define jsrun_launcher (1 << 7)             /* Launched via IBM's jsrun launcher */
#define lrun_launcher (1 << 8)              /* Launched via LLNL's wrappers around jsrun */

/* Possible values for startup_type, describe how Spindle servers are started */
#define startup_serial 0                    /* Job is non-parallel app, and server is forked/exec */
#define startup_lmon 1                      /* Start via LaunchMON */
#define startup_hostbin 2                   /* Start via hostbin */
#define startup_external 3                  /* An external mechanism starts Spindle */
#define startup_mpi 4                       /* Start via the MPI launcher */
#define startup_unknown 5                   /* Unknown launch mechanism */
#define startup_lsf 6                       /* LSF launcher from IBM*/

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

   /* The timeout when bundling messages before one is sent */
   unsigned int bundle_timeout_ms;

   /* The max size of a set of bundled messages before they are sent */
   unsigned int bundle_cachesize_kb;
} spindle_args_t;

/* Functions used to startup Spindle on the front-end. Init returns after finishing start-up,
   and it is the caller's responsibility to call spindleCloseFE when the servers terminate.
   spindleInitFE should be called with a filled in params and after the applications
   and daemons have been launched.*/
SPINDLE_EXPORT int spindleInitFE(const char **hosts, spindle_args_t *params);
SPINDLE_EXPORT int spindleCloseFE(spindle_args_t *params);

/* Optional call that blocks the FE thread until spindle servers indicate ready to exit.
   Mixing this with OPT_PERSIST could hang spindle */
SPINDLE_EXPORT int spindleWaitForCloseFE(spindle_args_t *params);
   
/* Fill in a spindle_args_t with default values */
SPINDLE_EXPORT void fillInSpindleArgsFE(spindle_args_t *params);

/* Similar to fillInSpindleArgsFE, but take overrides from a command line.  The command
     line options are the same ones that are seen under 'spindle --help'. 
   The sargc and sargv are an array and array length of command line arguments.  They
     can be empty.
   The options parameter is a bitfield of the below SPINDLE_FILLARGS_* arguments.  
   If parsing command arguments produces an error, then *errstr will be set to a malloc'd 
     string containing the error message.  
   Return 0 on success, and -1 if an error occurs.*/
#define SPINDLE_FILLARGS_NOUNIQUEID (1 << 0)  //Do not fill in the unique_id field
#define SPINDLE_FILLARGS_NONUMBER (1 << 1) //Do not fill in the number field
SPINDLE_EXPORT int fillInSpindleArgsCmdlineFE(spindle_args_t *params, unsigned int options, int sargc, char *sargv[],
                                              char **errstr);

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


/* If spindle is launched with OPT_RSHLAUNCH enabled, then this function will return 
   the pid of the rsh/ssh process at the top of the tree.  That process will terminate
   when the spindle session ends.

   This function only returns a valid value on the FE after spindleInitFE is called.

   This function return (pid_t) -1 on error.
*/
SPINDLE_EXPORT pid_t getRSHPidFE();

/* If spindle is launched with OPT_RSHLAUNCH enabled, then this function can be used
   to tell spindle if the top-of-tree RSH process has already been reaped via
   wait/waitpid by code above spindle_launch.  If the top-of-tree process has not 
   been reaped, then spindle will collect it in spindleCloseBE.
*/
SPINDLE_EXPORT void markRSHPidReapedFE();

/* Runs the server process on a BE, returns when server is done */
SPINDLE_EXPORT int spindleRunBE(unsigned int port, unsigned int num_ports, unique_id_t unique_id, int security_type,
                                int (*post_setup)(spindle_args_t *));

/* Optional interface, uses spindle to hook a subsequent exec to add the spindle args to the command line.
   Useful for resource managers that don't have hooks for changing application command lines.  The 
   execfilter string, if non-NULL, will only add args to execs where the executable has execfilter as
   a substring. */
SPINDLE_EXPORT int spindleHookSpindleArgsIntoExecBE(int spindle_argc, char **spindle_argv, char *execfilter);

/* Optional interface. If the opts parameter has OPT_BEEXIT set then shutdown will not trigger until
   each BE process/thread has had this call made on it.  This call does not need to be made on the same 
   process/thread as the one invoking spindleRunBE, but it does need to be made on the same host
   and with the 'location' from the spindle_args_t. */
SPINDLE_EXPORT int spindleExitBE(const char *location);


/* Adds output to spindle's debug loggging, if enabled, using printf-style interface.
   priority can be 1, 2, or 3.  More-verbose debugging output should be a higher 
   priority.
   Use the macro spindle_debug_printf rather than the spindle_debug_printf_impl function.
 */
#define spindle_debug_printf(PRIORITY, FORMAT, ...) \
   spindle_debug_printf_impl(PRIORITY, __FILE__, __LINE__, __func__, FORMAT, ## __VA_ARGS__)
     
SPINDLE_EXPORT int spindle_debug_printf_impl(int priority, const char *file, unsigned int line, const char *func, const char *format, ...);
   
/* Bitmask of values for the test_launchers parameter (no longer used) */
#define TEST_PRESETUP 1<<0
#define TEST_SERIAL   1<<1
#define TEST_SLURM    1<<2
#define TEST_FLUX     1<<3

#if defined(__cplusplus)
}
#endif

#endif

