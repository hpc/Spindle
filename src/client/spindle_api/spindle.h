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

#if !defined(SPINDLE_H_)
#define SPINDLE_H_

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(SPINDLE_INTERNAL_BUILD)
#define SPINDLE_EXPORT __attribute__((__visibility__("default")))
#else
#define SPINDLE_EXPORT
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>

int spindle_is_present() SPINDLE_EXPORT;

/**
 * These calls will perform IO operations through Spindle, if present,
 * and forward on to regular IO calls if Spindle is not present.
 * The open calls will only forward to Spindle if opening read-only.
 **/
int spindle_open(const char *pathname, int flags, ...) SPINDLE_EXPORT;
int spindle_stat(const char *path, struct stat *buf) SPINDLE_EXPORT;
int spindle_lstat(const char *path, struct stat *buf) SPINDLE_EXPORT;
FILE *spindle_fopen(const char *path, const char *mode) SPINDLE_EXPORT;

/**
 * If spindle is enabled through this API, then all open and stat calls
 * will automatically be routed through Spindle.
 *
 * enable_spindle and disable_spindle are counting calls that must be matched.
 * Thus if enable_spindle is called twice, disable_spindle must be called twice
 * to disable Spindle interception.  is_spindle_enabled returns the current count.
 *
 * Spindle enabling/disabling counts are tracked per-thread (if Spindle was built
 * with TLS support).
 **/
void enable_spindle() SPINDLE_EXPORT;
void disable_spindle() SPINDLE_EXPORT;
int is_spindle_enabled() SPINDLE_EXPORT;

/**
 * is_spindle_present returns true if the application was started under Spindle
 **/
int is_spindle_present() SPINDLE_EXPORT;

#if defined(__cplusplus)
}
#endif

#endif
