# Spindle in Docker

This directory contains a set of container recipes and scripts to allow you
to quickly bring up your own tiny cluster with [docker-compose](https://docs.docker.com/compose/install/), install
spindle, and give it a try. You will need both [docker-compose](https://docs.docker.com/compose/install/)
and [Docker](https://docs.docker.com/get-docker/) installed for this tutorial.

## 1. Build Containers

The [Dockerfile](Dockerfile) here is the base for building containers.
So running the build is as easy as:

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

## 2. Interact with spindle

Spindle should already be installed, and you can see the steps if you view the
[Dockerfile](Dockerfile).

```bash
# which spindle
/usr/local/bin/spindle
```

You can try running a job first without spindle:

```bash
$ srun -N 1 cat /proc/self/maps
```

If you try *with* spindle, this won't currently work:

```bash
$ spindle srun -N 1 cat /proc/self/maps
```

So instead you can get an allocation first:

```bash
$ salloc -N 1
```

If you want to view the source code, go to /spindle.

```bash
cd /spindle/testsuite
./runTests
```

## 3. Use Spindle

The first sanity check to see if spindle is working is to look at this output:

```bash
$ cat /proc/self/maps
[root@slurmctld /]# cat /proc/self/maps
00400000-0040b000 r-xp 00000000 00:7d 27450779                           /usr/bin/cat
0060b000-0060c000 r--p 0000b000 00:7d 27450779                           /usr/bin/cat
0060c000-0060d000 rw-p 0000c000 00:7d 27450779                           /usr/bin/cat
0189d000-018be000 rw-p 00000000 00:00 0                                  [heap]
7f2204c09000-7f2204dcb000 r-xp 00000000 00:7d 27466152                   /usr/lib64/libc-2.17.so
7f2204dcb000-7f2204fcb000 ---p 001c2000 00:7d 27466152                   /usr/lib64/libc-2.17.so
7f2204fcb000-7f2204fcf000 r--p 001c2000 00:7d 27466152                   /usr/lib64/libc-2.17.so
7f2204fcf000-7f2204fd1000 rw-p 001c6000 00:7d 27466152                   /usr/lib64/libc-2.17.so
7f2204fd1000-7f2204fd6000 rw-p 00000000 00:00 0 
7f2204fd6000-7f2204ff8000 r-xp 00000000 00:7d 27466129                   /usr/lib64/ld-2.17.so
7f2205064000-7f22051ed000 r--p 00000000 00:7d 27573565                   /usr/lib/locale/locale-archive
7f22051ed000-7f22051f0000 rw-p 00000000 00:00 0 
7f22051f6000-7f22051f7000 rw-p 00000000 00:00 0 
7f22051f7000-7f22051f8000 r--p 00021000 00:7d 27466129                   /usr/lib64/ld-2.17.so
7f22051f8000-7f22051f9000 rw-p 00022000 00:7d 27466129                   /usr/lib64/ld-2.17.so
7f22051f9000-7f22051fa000 rw-p 00000000 00:00 0 
7fff660dd000-7fff660fe000 rw-p 00000000 00:00 0                          [stack]
7fff661dc000-7fff661df000 r--p 00000000 00:00 0                          [vvar]
7fff661df000-7fff661e0000 r-xp 00000000 00:00 0                          [vdso]
ffffffffff600000-ffffffffff601000 --xp 00000000 00:00 0                  [vsyscall]
```

and compare that with the same command, but with spindle:

```bash
$ spindle --no-mpi cat /proc/self/maps
00400000-0040b000 r-xp 00000000 00:7d 28646903                           /tmp/spindle.84/usr/bin/1-spindlens-file-cat
0060b000-0060c000 r--p 0000b000 00:7d 27450779                           /usr/bin/cat
0060c000-0060d000 rw-p 0000c000 00:7d 27450779                           /usr/bin/cat
01dc8000-01de9000 rw-p 00000000 00:00 0                                  [heap]
7f8622f69000-7f86230f2000 r--p 00000000 00:7d 27573565                   /usr/lib/locale/locale-archive
7f86230f2000-7f86232b4000 r-xp 00000000 00:7d 27466152                   /usr/lib64/libc-2.17.so
7f86232b4000-7f86234b4000 ---p 001c2000 00:7d 27466152                   /usr/lib64/libc-2.17.so
7f86234b4000-7f86234b8000 r--p 001c2000 00:7d 27466152                   /usr/lib64/libc-2.17.so
7f86234b8000-7f86234ba000 rw-p 001c6000 00:7d 27466152                   /usr/lib64/libc-2.17.so
7f86234ba000-7f86234bf000 rw-p 00000000 00:00 0 
7f86234bf000-7f8623681000 r-xp 00000000 00:7d 27466152                   /usr/lib64/libc-2.17.so
7f8623681000-7f8623881000 ---p 001c2000 00:7d 27466152                   /usr/lib64/libc-2.17.so
7f8623881000-7f8623887000 rw-p 001c2000 00:7d 27466152                   /usr/lib64/libc-2.17.so
7f8623887000-7f862388c000 rw-p 00000000 00:00 0 
7f862388c000-7f86238a9000 r-xp 00000000 00:7d 28646902                   /tmp/spindle.84/usr/local/lib/spindle/0-spindlens-file-libspindle_audit_pipe.so
7f86238a9000-7f8623aa8000 ---p 0001d000 00:7d 28646902                   /tmp/spindle.84/usr/local/lib/spindle/0-spindlens-file-libspindle_audit_pipe.so
7f8623aa8000-7f8623aaa000 rw-p 0001c000 00:7d 28646902                   /tmp/spindle.84/usr/local/lib/spindle/0-spindlens-file-libspindle_audit_pipe.so
7f8623aaa000-7f8623aad000 rw-p 00000000 00:00 0 
7f8623aad000-7f8623acf000 r-xp 00000000 00:7d 27466129                   /usr/lib64/ld-2.17.so
7f8623bc3000-7f8623cc5000 rw-p 00000000 00:00 0 
7f8623ccb000-7f8623cce000 rw-p 00000000 00:00 0 
7f8623cce000-7f8623cd0000 rw-p 00021000 00:7d 27466129                   /usr/lib64/ld-2.17.so
7f8623cd0000-7f8623cd1000 rw-p 00000000 00:00 0 
7ffd73932000-7ffd73953000 rw-p 00000000 00:00 0                          [stack]
7ffd739bd000-7ffd739c0000 r--p 00000000 00:00 0                          [vvar]
7ffd739c0000-7ffd739c1000 r-xp 00000000 00:00 0                          [vdso]
ffffffffff600000-ffffffffff601000 --xp 00000000 00:00 0                  [vsyscall]
```

You should see several paths replaced with spindle ones.

Next, let's try running a benchmarking tool with and without spindle.
This benchmark is called "Pynamic." Let's first clone it:

```bash
$ git clone https://github.com/LLNL/pynamic
$ cd pynamic/pynamic-pyMPI-2.6a1

# Run python config_pynamic.py to see usage
```
And then we would build shared libraries as follows. We are doing to decrease
from the default because it will take forever!

```bash
# usage: config_pynamic.py <num_files> <avg_num_functions> [options] [-c <configure_options>]
# example: config_pynamic.py 900 1250 -e -u 350 1250 -n 150
# <num_files> = total number of shared objects to produce
# <avg_num_functions> = average number of functions per shared object
$ python config_pynamic.py 900 1250 -e -u 350 1250 -n 150
```

Don't actually do that - it will never finish and control+C won't kill it!
Try this one instead, with a timer

```bash
$ time python config_pynamic.py 30 1250 -e -u 350 1250 -n 150

************************************************
summary of pynamic-sdb-pyMPI executable and 10 shared libraries
Size of aggregate total of shared libraries: 2.5MB
Size of aggregate texts of shared libraries: 6.8MB
Size of aggregate data of shared libraries: 408.4KB
Size of aggregate debug sections of shared libraries: 0B
Size of aggregate symbol tables of shared libraries: 0B
Size of aggregate string table size of shared libraries: 0B
************************************************

real	21m33.556s
user	14m54.538s
sys	3m31.206s
```

The above does take a bit (as you can see from the time) so let's try it now with
spindle:

```bash
$ time spindle python config_pynamic.py 30 1250 -e -u 350 1250 -n 150
```

**under development, not written yet, debugging things!**

```
  3.1 TO TEST
    % python pynamic_driver.py `date +%s`

      or in a batchxterm:

    % srun pyMPI pynamic_driver.py `date +%s`

    % srun pynamic-pyMPI pynamic_driver.py `date +%s`

    % srun pynamic-sdb-pyMPI pynamic_driver.py `date +%s`

    % srun pynamic-bigexe pynamic_driver.py `date +%s`

    # note: Pynamic creates 3 executables:
    #    pyMPI - a vanilla pyMPI build
    #    pynamic-pyMPI - pyMPI with the generated .so's linked in
    #    pynamic-sdb-pyMPI - pyMPI with the generated libraries statically linked in
    # and 2 optional executables (with the -b flag)
    #    pynamic-bigexe-pyMPI - a larger pyMPI with the generated .so's linked in
    #    pynamic-bigexe-sdb-pyMPI - a larger pyMPI with the generated libraries staically linked in

--------------------------------------------------------
4. CONTACTS
   Greg Lee <lee218@llnl.gov>	
   Dong Ahn <ahn1@llnl.gov>
   Bronis de Supinski <desupinski1@llnl.gov>
   John Gyllenhaal <gyllenhaal1@llnl.gov>
 
# run the pynamic benchmark with and without spindle
/cat/proc/self/maps

prints out for each library loaded and bincat parts of address space takes up
run same command under spindle

spindle --no-mpi cat /proc/self/maps

to check if install works and is visible outside of spindle itself
/proc/pid/maps

https://github.com/LLNL/pynamic
```


## 4. Clean Up

When you are done, exit from the container, stop and remove your images:

```bash
$ docker-compose stop
$ docker-compose rm
```
