# Spindle in Docker

This directory contains a set of container recipes and scripts to allow you
to quickly bring up your own tiny cluster with [docker-compose](https://docs.docker.com/compose/install/), install
spindle, and give it a try. You will need both [docker-compose](https://docs.docker.com/compose/install/)
and [Docker](https://docs.docker.com/get-docker/) installed for this tutorial.

## 1. Build Containers

First, let's build a base container with slurm and centos with the [Dockerfile](Dockerfile) here:

```bash
$ docker build -t vanessa/slurm:20.11.8 .
```
Then building containers is as easy as:

```bash
$ docker-compose build
```

And then bringing them up:

```bash
$ docker-compose up -d
```

And checking that they are running

```bash
$ docker-compose ps
  Name                 Command               State          Ports       
------------------------------------------------------------------------
c1          /usr/local/bin/docker-entr ...   Up      6818/tcp           
c2          /usr/local/bin/docker-entr ...   Up      6818/tcp           
mysql       docker-entrypoint.sh mysqld      Up      3306/tcp, 33060/tcp
slurmctld   /usr/local/bin/docker-entr ...   Up      6817/tcp           
slurmdbd    /usr/local/bin/docker-entr ...   Up      6819/tcp           
```

Each of c1 and c2 are nodes for our cluster, and then slurmctld is like the login node.

```bash
$ docker exec -it slurmctld bash
```

Try running a job!

```bash
$ sbatch --wrap="sleep 20"
# squeue
             JOBID PARTITION     NAME     USER ST       TIME  NODES NODELIST(REASON)
                 1    normal     wrap     root  R       0:00      1 c1
```

## 2. Install spindle

Now let's follow instructions to install spindle.

```bash
$ git clone https://github.com/hpc/spindle
$ cd spindle
```

We want to install providing paths to munge and slurm.

```bash
./configure --with-munge-dir=/etc/munge --enable-sec-munge --with-slurm-dir=/etc/slurm --enable-testsuite=no
make
make install
```

Note that we are disabling the test suite otherwise we'd get an install error not detecting
an MPI library. Now we can see spindle!

```
# spindle --help
Usage: spindle [OPTION...] mpi_command

 These options specify what types of files should be loaded through the Spindle
 network
  -a, --reloc-aout=yes|no    Relocate the main executable through Spindle.
                             Default: yes
  -f, --follow-fork=yes|no   Relocate objects in fork'd child processes.
                             Default: yes
  -l, --reloc-libs=yes|no    Relocate shared libraries through Spindle.
                             Default: yes
  -x, --reloc-exec=yes|no    Relocate the targets of exec/execv/execve/...
                             calls. Default: yes
  -y, --reloc-python=yes|no  Relocate python modules (.py/.pyc) files when
                             loaded via python. Default: yes

 These options specify how the Spindle network should distibute files.  Push is
 better for SPMD programs.  Pull is better for MPMD programs. Default is push.
  -p, --push                 Use a push model where objects loaded by any
                             process are made available to all processes
  -q, --pull                 Use a pull model where objects are only made
                             available to processes that require them

 These options configure Spindle's network model.  Typical Spindle runs should
 not need to set these.
  -c, --cobo                 Use a tree-based cobo network for distributing
                             objects
  -t, --port=port1-port2     TCP/IP port range for Spindle servers.  Default:
                             21940-21964

 These options specify the security model Spindle should use for validating TCP
 connections. Spindle will choose a default value if no option is specified.
      --security-munge       Use munge for security authentication

 These options specify the job launcher Spindle is being run with.  If
 unspecified, Spindle will try to autodetect.
      --launcher-startup     Launch spindle daemons using the system's job
                             launcher (requires an already set-up session).
      --no-mpi               Run serial jobs instead of MPI job
      --openmpi              MPI job is launched with the OpenMPI job jauncher.
                            
      --slurm                MPI job is launched with the srun job launcher.
      --wreck                MPI Job is launched with the wreck job launcher.

 Options for managing sessions, which can run multiple jobs out of one spindle
 cache.
      --end-session=session-id   End a persistent Spindle session with the
                             given session-id
      --run-in-session=session-id
                             Run a new job in the given session
      --start-session        Start a persistent Spindle session and print the
                             session-id to stdout

 Misc options
  -b, --shmcache-size=size   Size of client shared memory cache in kilobytes,
                             which can be used to improve performance if
                             multiple processes are running on each node.
                             Default: 0
      --cache-prefix=path    Alias for python-prefix
      --cleanup-proc=yes|no  Fork a dedicated process to clean-up files
                             post-spindle.  Useful for high-fault situations.
                             Default: no
  -d, --debug=yes|no         If yes, hide spindle from debuggers so they think
                             libraries come from the original locations.  May
                             cause extra overhead. Default: yes
  -e, --preload=FILE         Provides a text file containing a white-space
                             separated list of files that should be relocated
                             to each node before execution begins
      --enable-rsh=yes|no    Enable startint daemons with an rsh tree, if the
                             startup mode supports it. Default: No
      --hostbin=EXECUTABLE   Path to a script that returns the hostlist for a
                             job on a cluster
  -h, --no-hide              Don't hide spindle file descriptors from
                             application
  -k, --audit-type=subaudit|audit
                             Use the new-style subaudit interface for
                             intercepting ld.so, or the old-style audit
                             interface.  The subaudit option reduces memory
                             overhead, but is more complex.  Default is audit.
      --msgcache-buffer=size Enables message buffering if size is non-zero,
                             otherwise sets the size of the buffer in
                             kilobytes
      --msgcache-timeout=timeout   Enables message buffering if size is
                             non-zero, otherwise sets the buffering timeout in
                             milliseconds
  -n, --noclean=yes|no       Don't remove local file cache after execution.
                             Default: no (removes the cache)
  -o, --location=directory   Back-end directory for storing relocated files.
                             Should be a non-shared location such as a ramdisk.
                              Default: $TMPDIR
      --persist=yes|no       Allow spindle servers to persist after the last
                             client job has exited. Default: No
  -r, --python-prefix=path   Colon-seperated list of directories that contain
                             the python install location
  -s, --strip=yes|no         Strip debug and symbol information from binaries
                             before distributing them. Default: yes

  -?, --help                 Give this help list
      --usage                Give a short usage message
  -V, --version              Print program version

Mandatory or optional arguments to long options are also mandatory or optional
for any corresponding short options.

Report bugs to legendre1@llnl.gov.
```

## 3. Use Spindle

**TODO** we need a dummy example here


## 4. Clean Up

When you are done, exit from the container, stop and remove your images:

```bash
$ docker-compose stop
$ docker-compose rm
```
