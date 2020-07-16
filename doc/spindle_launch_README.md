Spindle Job Launch Integration API
==================================

This README describes the API for integrating Spindle into job launchers
(i.e., making Spindle an option to mpirun). End-users of Spindle should
not need to use this API.

Overview
--------

While Spindle comes with several mechanisms for launching (launchmon,
hostbin, slurm, and serial as of v0.13), each of these has disadvantages such
as in-operatability with debuggers (launchmon), difficult to use
(hostbin), or of limited scope (serial). In the ideal case Spindle
should be integrated directly into a job launcher, which can easily
perform the necessary actions of starting the servers, managing the job
lifetime, and helping Spindle establish connections. This document
describes the API for integrating Spindle into job launchers.

API
---

The API is split into two parts:

1.  The front-end (FE) API, which is expected to be used on a
    job-control node or someplace where one can modify and query the job
    information (such as the application command line and host list)
    before it runs. One instance of this API should be invoked for each
    job Spindle will control. This API need not be run a node where the
    application will run (though it could).
2.  The back-end (BE) API, which should be run in a daemon on each node
    in the job (i.e., if a job has 16k processes spread across 1k nodes,
    then you'd have 1k daemons invoking this API).

Both API's are declared in the
`$SPINDLE_PREFIX/include/spindle_launch.h` header file. The FE
functions are defined in the `$SPINDLE_PREFIX/lib/libspindlefe.[a,so]`
libraries and all have a "FE" suffix on their name. The BE functions
are defined in the `$SPINDLE_PREFIX/lib/libspindlebe.[a,so]` libraries
and all have a "BE" suffix on their name. Both APIs utilize common
data types, which are also defined in the `spindle_launch.h` header file.

The Datatypes
-------------

The `spindle_launch.h` include file contains structures, typedefs,
macros, and defines used by both the FE and BE APIs. Users should
utilize the aliases and names described in this README rather than the
underlying types and values (i.e., type the options bitflag as an
`opt_t`, rather then using the current underlying typedef of `uint64_t`).
This should keep this interface API compatible, even in situations where
we have to break ABI compatibility.

- `typedef ... unique_id_t`

    The `unique_id_t` is an integer type used to identify a specific Spindle
    session. If a user is running multiple jobs on overlapping nodes, then
    the `unique_id_t` will be used to distinguish the Spindle sessions for
    each job.

- `typedef ... opt_t`

    The `opt_t` type is a bitfield describing the options enabled in this
    Spindle session. It will have the following bit values set if spindle
    should:

    -   `OPT_COBO` - Use COBO for the communication implementation
    -   `OPT_DEBUG` - Hide from debuggers (currently unnecessary)
    -   `OPT_FOLLOWFORK` -Follow forks and manage child processes
    -   `OPT_PRELOAD` - Pre-stage libraries from a preload file
    -   `OPT_PUSH` - Stage files to all nodes when one node requests one
    -   `OPT_PULL` - Stage files only to nodes that specifically request
        them
    -   `OPT_RELOCAOUT` - Stage the initial executable
    -   `OPT_RELOCSO` - Stage shared libraries
    -   `OPT_RELOCEXEC` - Stage the targets of exec() calls
    -   `OPT_RELOCPY` - Stage python .py/.pyc/.pyo files
    -   `OPT_STRIP` - Strip debug information from ELF files before staging
    -   `OPT_NOCLEAN` - Not clean stage area on exit (useful for debugging)
    -   `OPT_NOHIDE` - Hide Spindle's communication FDs from application
    -   `OPT_REMAPEXEC` - Use a remapping hack to make /proc/PID/exe point
        to original exe
    -   `OPT_LOGUSAGE` - Log usage information to a file
    -   `OPT_SHMCACHE` - Use a shared memory cache optimization (only needed
        on BlueGene)
    -   `OPT_SUBAUDIT` - Use the subaudit mechanism (needed on BlueGene and
        very old GLIBCs)
    -   `OPT_PERSIST` - Spindle servers should remain running after all
        clients exit.
    -   `OPT_SEC` - Security mode, set to one of the below `OPT_SEC_*`
        values:
        -   `OPT_SEC_MUNGE` - Use MUNGE to generate connection
            authentication keys
        -   `OPT_SEC_KEYLMON` - Use LaunchMON to distribute security keys
            (not yet implemented)
        -   `OPT_SEC_KEYFILE` - Generate a key and distribute it via a
            shared file system
        -   `OPT_SEC_NONE` - Do not authenticate connections between Spindle
            components

        Since `OPT_SEC` is a multi-bit value, it should be accessed with the
        following macros:

        -   `OPT_SET_SEC(OPT, X)` - Used to set the OPT_SEC part of the
            opt_t bitfield to the value X. OPT should be an opt_t and X
            should be a OPT_SEC_* value.
        -   `OPT_GET_SEC(OPT)` - Returns the OPT_SEC part of the opt_t
            bitfield specified by OPT.
   -    'OPT_SESSION' - Run in session mode, where spindle caches persist
        between runs.
   -    'OPT_MSGBUNDLE' - Enable message bundling, which bundles spindle's
        small network packets into larger packets.  Can be useful for
        high-latency networks.
   -    'OPT_BEEXIT' - Spindle will block its exit until each BE has
        had 'spindleExitBE()' called on it.

- `typedef struct { ... } spindle_args_t`

    The above opt_t bitfield cannot describe all the configurations of a
    Spindle session (such as a TCP port number), and that information is
    supplemented in the `spindle_args_t` structure. This struct's member
    types and names are:

    -   `unsigned int number` - A public-facing session ID. In future
        versions this may be merged with unique_id.
    -   `unsigned int port` - A first TCP port number that Spindle servers
        will try to use for communication.
    -   `unsigned int num_ports` - Along with port, specifies set of ports
        that Spindle can use for communication. If another application is
        occupying the TCP port, Spindle will try another port in the range
        between port and port+num_ports. For example, if port is 21940 and
        num_ports is 25, then Spindle will try TCP ports 21940 to 21964.
    -   `opt_t opts` - A bitfield of the above opt_t values.
    -   `unique_id_t unique_id` - A private-facing session ID that should be
        common between all FE and BE servers in this instance.
    -   `unsigned int use_launcher` - A field that specifies what kind of
        application launcher is being used. It will be one of the following
        values:
        -   `srun_launcher` - SLURM is the job launcher
        -   `serial_launcher` - This is a non-parallel job launched via
            fork/exec
        -   `openmpi_launcher` - ORTE is the job launcher
        -   `wreckrun_launcher` - FLUX is the job launcher
        -   `marker_launcher` - An unknown job launcher is utilizing Spindle
            markers in the launch line
        -   `external_launcher` - An external job launcher is handling the
            application. *This is the value that should be used with this
            API*.
        -   `slurm_plugin_launcher` - Spindle was launched by the Slurm plugin.
    -   `unsigned int startup_type` - A field specifying how spindle servers
        are being launched. It should be one of the following values:
        -   `startup_serial` - The job is non-parallel and Spindle should
            start servers via fork/exec
        -   `startup_lmon` - LaunchMON will be used to start the Spindle
            servers.
        -   `startup_hostbin` - The hostbin mechanism will be used to start
            Spindle servers.
        -   `startup_external` - External infrastructure will be used to
            start Spindle servers. *This is the value that should be used
            with this API*.
        -   `unsigned int shm_cache_size` - When using a shared memory cache
        between clients, this is the size in bytes of the cache. This is
        only used on BlueGene systems.
    -   `char *location` - The staging directory where Spindle should store
        relocated files. This should preferably be on local and scalable
        storage, such as a RAMDISK or SSD. You can use environment variables
        in this string by encoding them with a `$`. i.e.,
        `$TMPDIR/spindle`
    -   `char *pythonprefix` - Spindle can do more efficient Python loading
        if it knows the install prefix of Python. This string should be a
        ':' separated list of directories that are prefixes of Python
        installs. It is legal to use a prefix at an arbitrary point above
        Python in the directory tree (e.g, `/usr` will capture the python
        install in `/usr/lib64/python2.7/`), but the job should never be
        doing file writes in one of these prefixes (so `/` or `/home`
        would make bad prefixes).
    -   `char *preloadfile` - Points to a file containing a white-space
        separated list of files that should be staged onto every node in the
        job before the application runs.

The FrontEnd API
----------------

As mentioned above, functions in the FE API are expected to be run from
a job-control node, or someplace similar. It should be somewhere with
the ability to modify the application launch line and provide the list
of hostnames used in the job. The FE API functions are:

-   `void fillInSpindleArgsFE(spindle_args_t *params)`

    This function fills in the 'params' struct with what Spindle
    considers sensible default values. These default values may come
    from configure-time arguments used to build Spindle (i.e., if a
    default port was specified at configure time), from hardcoded
    values, or from runtime detected values.

    Calling this function is optional, but recommended. API users may
    rely on this function to fill-in options and then overwrite any
    important values, or just manually fill in the entire
    `spindle_args_t` structure.

-   `int fillInSpindleArgsCmdLineFE(spindle_args_t *params, unsigned int options,
     int sargc, char *sargv[], char **errstr)`

    This function is similar to fillInSpindleArgsFE, but it takes extra options
    and command line arguments.

    The command line arguments are specified by `sargs` and `sargv`, and are a
    set of command line options that could be validly passed to the 'spindle'
    executable.  The `params` will be filled out with values from these
    arguments.  For example, passing ["--port", "1000", "--pull"] will fill
    in the `params` to use port 1000 and run spindle in it's pull mode.  An
    executable name, which would normally be passed in sargv[0], should not be
    included when calling this function.

    The `options` argument is a bitfield of arguments that control how the
    `params` argument is filled in.  Valid values are:

     - `SPINDLE_FILLARGS_NOUNIQUEID` - The uniqueid field of `params`
       field will not be filled in.
     - `SPINDLE_FILLARGS_NONUMBER` - The number field of `params` will not
        be filled in.  Since other fields in params may depend on number,
        the `params->number` field should be filled in before calling
        `fillInSpindleArgsCmdLineFE()`

    These values can be bitwise or'd together.

    The `errstr` parameter will be set to a error message string if there
    is a problem parsing the arguments passed in sargv and sargc.

    This function return 0 on success, and non-zero on error.    

-   `int getApplicationArgsFE(spindle_args_t *params, int *spindle_argc, char spindle_argv)`

    Given a filled in `spindle_args_t`, `getApplicationArgsFE` gets the
    array of arguments that should be inserted into the application
    launch line. The job launcher is responsible for inserting these new
    arguments into the application launch line before the executable.

    Upon completion, `*spindle_argv` will be set to point at a malloc'd
    array of strings (each of which is also malloc'd), that should be
    inserted. `*spindle_argc` will be set to the size of the
    `*spindle_argv` array. `spindle_argv` and its strings can be free'd
    at any time.

    For example, the line:

    `mpirun -n 512 mpi_app`

    Should be changed to:

    `mpirun -n 512 SPINDLE_ARGV[] mpi_app`

    This function return 0 on success and non-0 on error.

-   `int spindleInitFE(const char **hosts, spindle_args_t *params)`

    This function initializes the Spindle front-end's network
    connection to the servers running on the back-ends. It should be
    called with a filled-in `spindle_args_t`, params, and given the list
    of hostnames in the job, hosts. The hosts parameter is an array of
    strings with a NULL pointer in the final array slot. `spindleInitFE`
    does not keep pointers into the hosts table, and it may be
    deallocated after `spindleInitFE` returns.

    `spindleInitFE` will block while establishing connections to
    Spindle's servers, and thus should be run concurrently to the
    `spindleRunBE()` function on the BEs. It will return after the initial
    connection has been established.

    This function return 0 on success and non-0 on error.

-   `int spindleWaitForCloseFE(spindle_args_t *params)`

    This function blocks the calling thread until the spindle session
    associated with the `params` argument is ready to exit.  This
    function can be optionally used to block a thread between `spindleInitFE()`
    and `spindleCloseFE()`, if no other work was needed by that thread.

    This function return 0 on success and non-0 on error.
    
-   `int spindleCloseFE(spindle_args_t *params)`

    This function both shuts down and cleans the current Spindle
    session. It takes a filled-in `spindle_args_t`, params, as input. If
    Spindle is actively running alongside a job when this function this
    is called it will force-shutdown the Spindle servers (which may have
    a negative impact on the application). This function will also clean
    up temporary files and sockets associated with the Spindle FE.

    This function return 0 on success and non-0 on error.

The BackEnd API
---------------

The BackEnd API is expected to be invoked on each host in the job. It
currently consists of a single function, which starts the Spindle
server. The inputs to the the BE API can be found in the
`spindle_args_t` used on the front-end, but those input values will need
to be broadcast from the FE to each node by the job launcher. The BE API
function is:

-   `int spindleRunBE(unsigned int port, unsigned int num_ports, unique_id_t
     unique_id, int security_type, int (*post_setup)(spindle_args_t *))`

    This function starts the spindle server running. As input it takes
    the `port`, `num_ports`, `unique_id`, which should have the same-named
    values as passed to `spindleInitFE` in the `spindle_args` parameter.
    The `security_type` should match the `OPT_SEC` value passed to
    `spindleInitFE` in the `opt` field of `spindle_args` (i.e.,
    `OPT_SEC_MUNGE`, `OPT_SEC_KEYFILE`, ...).

    This function also takes an callback function, `post_setup`, which
    will be invoked Spindle is ready to accept connections from
    applications. The callback is passed a filled in `spindle_args_t`
    with the same values as the front-end's. Using this callback is
    optional, it can safely be a NULL value, and it is not necessary to
    synchronize application startup against this callback.

    The `SpindleRunBE` function does not return until the Spindle server
    terminates, which will likely happen at job completion. Therefore
    `SpindleRunBE` should be invoked in either a forked process or
    separate thread if the job launcher does not want to remain blocked.

    This function return 0 on success and non-0 on error.
    
-   `int spindleHookSpindleArgsIntoExecBE(int spindle_argc, char **spindle_argv,
     char *execfilter)`

    This optional function can be used to auto-magically insert spindle's
    bootstrap arguments into a future `exec()`/`execv()`/`execve()`/... call
    that would invoke an application.

    This function is useful if a resource manager does not provide a way to
    insert Spindle's bootstrap arguments into an application `exec()` call.
    Spindle will use its function wrapping capabilities to modify the current
    process and intercept any calls to the `exec()` family.  If that call
    is `exec()`'ing the application, then spindle will insert its bootstrap
    arguments into the exec.
    
    The `spindle_argc` and `spindle_argv` arguments should be the values 
    returned from `getApplicationArgsFE()`.

    The `execfilter` string controls which applications should be wrapped.
    If `execfilter` is non-NULL, Spindle will only insert bootstrap
    arguments when the executable part of the name matches `execfilter`.
    For example, if the `exec()` call is invoking `/usr/bin/ls`, and
    `execfilter` is `ls`, then spindle will insert bootstrap arguments.
    If `execfilter` is NULL, then all `exec()` calls will be bootstrapped by
    spindle.

    This function return 0 on success and non-0 on error.
    
-  `int spindleExitBE(const char *location)`

    This optional call is used with the `OPT_BEEXIT` option.  When `OPT_BEEXIT`
    is enabled, spindle will block BE servers from exiting until each one
    has had `spindleExitBE()` called on it.  `spindleExitBE()` can be called
    from a different process/thread from the thread running `spindleRunBE()`,
    but it needs to be called on the same host.

    The `location` parameter should be the value of the `location` field
    of the `spindle_args_t` associated with the session.

    This function return 0 on success and non-0 on error.

Common API Functions
---------------

-  `int spindle_debug_printf(int priority, const char *format, ...)`

   This routine inserts printf-style messages into Spindle's debug logs.

   Debug logs are typically enabled by setting the SPINDLE_DEBUG environment
   variable to "1", "2", or "3" (where "1" produces less output, and "3" produces
   more output).  This causes a single-file-per-host of debug info to be written
   into $PWD.  Any messages printed through `spindle_debug_printf()` will be
   interleaved into these logs, if they are being generated.

   The `priority` parameter signifies the verbosity level of the current message.
   The message will be added to the logs if SPINDLE_DEBUG is equal-or-greater-than
   `priority`.  It should be a value of 1, 2, or 3 with higher-level less-verbose
   debug messages getting 1 and lower-level more-verbose messages getting 3.

   The 'format' and 'varargs' have the same semantics as the system's underlying
   'printf()' routine.

   This function returns the number of characters printed, similar to printf.  Spindle
   may add extra provenance characters, which will be included in the return value. 

   

   
