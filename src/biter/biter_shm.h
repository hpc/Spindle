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

#if !defined(BITER_SHM_H_)
#define BITER_SHM_H_

/**
 * We try to keep the client library only dependant on glibc (because
 * anything it depends on must be loaded via traditional file system
 * access).  Unfortunately, the shm_open and shm_unlink functions
 * come from librt.so.  So we've got our own implementations.
 **/
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int biter_shm_open(const char *name, int oflag, mode_t mode);
int biter_shm_unlink(const char *name);

#endif
