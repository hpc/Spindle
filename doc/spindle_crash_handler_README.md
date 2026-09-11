Spindle Crash Handler
=====================

This README describes Spindle's crash handling features, which control
coredump creation and report crash locations when an application crashes.

Overview
--------

When a large parallel application hits a bug, many processes tend to
crash at the same place.  If coredumps are enabled, every crashing
process writes its own coredump, which can flood a shared file system
with thousands of near-identical files.

With crash handling enabled, each crashing process reports its crash
location to Spindle.  Spindle groups the reports by crash site and
selects one process per unique site to write a coredump; coredumps
from the other processes at that site are suppressed. Spindle can 
also write a crash log summarizing the crash sites and which ranks
crashed at each.

Crash handling covers crashes from SIGSEGV, SIGBUS, SIGFPE, SIGILL,
and SIGABRT. 

Usage
-----

Pass `--crash-dedup` to the spindle command to enable coredump
deduplication:

```
    spindle --crash-dedup srun -n 512 ./my_app
```

Add `--crash-log` to also write a crash log.  Since `--crash-log`
implies `--crash-dedup`, it can be used on its own:

```
    spindle --crash-log=/p/lustre/me/logs srun -n 512 ./my_app
```

As with other coredumps, the usual system settings apply.  Make sure
the core file size limit (`ulimit -c`) allows coredumps on the compute
nodes, or the selected process will not be able to write one.

When Spindle runs through a resource manager plugin rather than the
spindle command, pass the same options through the plugin.  With the
Slurm plugin:

```
    srun --spindle="--crash-log" -n 512 ./my_app
```

With a Slurm plugin session, give the options when the session starts
and they apply to every job run in that session:

```
    salloc -N4 --spindle-session="--crash-log" ./my_jobs.sh
```

With the Flux plugin, use the corresponding shell options:

```
    flux run -o spindle.crash-dedup -o spindle.crash-log -N4 -n512 ./my_app
```

Crash sites
-----------

A crash site identifies where the application crashed.  It has two
parts: the executable, and the location of the fault.  For most signals
the location is the library and offset of the faulting instruction:

```
    /home/me/my_app        libfoo.so.1+0x2f10
```

For SIGABRT crashes that carry a glibc abort message, such as a failed
`assert()` or a heap corruption report, the abort message is the
location:

```
    /home/me/my_app        abort:my_app: solver.c:88: solve: Assertion `n > 0' failed.
```

Two processes are at the same crash site only when both parts match, so
the same library fault under two different executbales counts as two crash sites
and produces two coredumps.

The crash log
-------------

The `--crash-log[=PATH]` option writes a log describing every crash in
the job.  If `PATH` is omitted, the log is written to
`spindle-crash-log.$NUMBER` in the working directory, where NUMBER is
Spindle's session number.  `PATH` may name the log file itself or an
existing directory to place the default filename in.  Environment
variables can be used in `PATH` by prefixing them with a `$`
character.

The log is a CSV file with a header line and one line per crashing
rank:

```
    rank,exemplar,exe,site,corepath
    4,4,/home/me/my_app,libsolver.so.1+0x2f10,/p/lustre/me/run/core.my_app.19204
    5,4,/home/me/my_app,libsolver.so.1+0x2f10,/p/lustre/me/run/core.my_app.19204
    6,4,/home/me/my_app,libsolver.so.1+0x2f10,/p/lustre/me/run/core.my_app.19204
    0,0,/home/me/my_app,abort:my_app: solver.c:88: solve: Assertion `n > 0' failed.\n,/p/lustre/me/run/core.my_app.19200
    1,0,/home/me/my_app,abort:my_app: solver.c:88: solve: Assertion `n > 0' failed.\n,/p/lustre/me/run/core.my_app.19200
```

The columns are:

- `rank`: the rank that crashed, as numbered by the launcher (for
    example, `PMIX_RANK`, `SLURM_PROCID`, or `FLUX_TASK_RANK`).
- `exemplar`: the rank selected to write the coredump for the
    crash site.  
- `exe`: the path to the executable that crashed.
- `site`: the library+offset or abort message of the crash.
- `corepath`: the path to the core file for the given crash site.

The log is written when the job exits, or at session end when running
in session mode.  If no process crashed, no log file is created.

Interaction with application signal handlers
--------------------------------------------

Applications that install their own handlers for crash signals keep
working under Spindle.  When a crash signal arrives, Spindle invokes
the application's handler first.  Some runtimes handle faults as part
of normal operation, such as garbage collectors that trap writes
to protected pages. If the application's handler resolves the fault,
Spindle does not handle the signal and execution continues.

Stack overflows
---------------

A crash caused by stack overflow cannot normally run a signal
handler, because the handler has no stack to run on.  The
`--crash-altstack` option makes Spindle's crash handling run on an
alternate signal stack, so stack-overflow crashes on the
application's main thread can be handled and deduplicated like any
other crash. This will cause any application-registered signal handlers for
SIGSEGV, SIGBUS, SIGFPE, SIGILL, or SIGABRT to also run on the alternate
stack, which may alter application behavior. This option is off by default.

