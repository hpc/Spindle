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

#if !defined(SHMCACHE_H_)
#define SHMCACHE_H_

#include <stdlib.h>

int shmcache_lookup_or_add(const char *libname, char **result);
int shmcache_add(const char *libname, const char *mapped_name);
int shmcache_update(const char *libname, const char *mapped_name);
int shmcache_init(const char *tmpdir, int unique_number, size_t shm_size, size_t hlimit);
int shmcache_waitfor_update(const char *libname, char **result);
void shmcache_take_lock();
void shmcache_release_lock();

extern char *in_progress;

#endif
