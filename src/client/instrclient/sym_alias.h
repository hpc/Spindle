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

#if !defined(SYM_ALIAS_H_)
#define SYM_ALIAS_H_

#include "intercept.h"

#if defined(INTERCEPT_OPEN)
int open(const char *path, int oflag, ...) __attribute__ ((alias ("rtcache_open"), __visibility__("default")));
int open64(const char *path, int oflag, ...) __attribute__ ((alias ("rtcache_open64"), __visibility__("default")));
FILE *fopen(const char *path, const char *mode) __attribute__ ((alias ("rtcache_fopen"), __visibility__("default")));
FILE *fopen64(const char *path, const char *mode) __attribute__ ((alias ("rtcache_fopen64"), __visibility__("default")));
int close(int fd) __attribute__ ((alias ("rtcache_close"), __visibility__("default")));
#endif

#if defined(INTERCEPT_STAT)
int stat(const char *path, struct stat *buf) __attribute__ ((alias ("rtcache_stat"), __visibility__("default"))); 
int lstat(const char *path, struct stat *buf) __attribute__ ((alias ("rtcache_lstat"), __visibility__("default")));
int __xstat(int vers, const char *path, struct stat *buf) __attribute__ ((alias ("rtcache_xstat"), __visibility__("default")));
int __xstat64(int vers, const char *path, struct stat *buf) __attribute__ ((alias ("rtcache_xstat64"), __visibility__("default")));
int __lxstat(int vers, const char *path, struct stat *buf) __attribute__ ((alias ("rtcache_lxstat"), __visibility__("default")));
int __lxstat64(int vers, const char *path, struct stat *buf) __attribute__ ((alias ("rtcache_lxstat64"), __visibility__("default")));
int fstat(int fd, struct stat *buf) __attribute__ ((alias ("rtcache_fstat"), __visibility__("default")));
int __fxstat(int vers, int fd, struct stat *buf) __attribute__ ((alias ("rtcache_fxstat"), __visibility__("default")));
int __fxstat64(int vers, int fd, struct stat *buf) __attribute__ ((alias ("rtcache_fxstat64"), __visibility__("default")));
#endif

#if defined(INTERCEPT_EXEC)
int execl(const char *path, const char *arg, ...) __attribute__ ((alias ("execl_wrapper"), __visibility__("default")));
int execv(const char *path, char *const argv[]) __attribute__ ((alias ("execv_wrapper"), __visibility__("default")));
int execle(const char *path, const char *arg0, ...) __attribute__ ((alias ("execle_wrapper"), __visibility__("default")));
int execve(const char *path, char *const argv[], char *const envp[]) __attribute__ ((alias ("execve_wrapper"), __visibility__("default")));
int execlp(const char *path, const char *arg0, ...) __attribute__ ((alias ("execlp_wrapper"), __visibility__("default")));
int execvp(const char *path, char *const argv[]) __attribute__ ((alias ("execvp_wrapper"), __visibility__("default")));
pid_t vfork() __attribute__ ((alias ("vfork_wrapper"), __visibility__("default")));
#endif

#if defined(INTERCEPT_SPINDLEAPI)
int spindle_open(const char *pathname, int flags, ...) __attribute__ (( alias ("int_spindle_open"), __visibility__("default")));
FILE *spindle_fopen(const char *path, const char *opts) __attribute__ (( alias ("int_spindle_fopen"), __visibility__("default")));
int spindle_stat(const char *path, struct stat *buf) __attribute__ (( alias ("int_spindle_stat"), __visibility__("default")));
int spindle_lstat(const char *path, struct stat *buf) __attribute__ (( alias ("int_spindle_lstat"), __visibility__("default")));
int spindle_is_present() __attribute__ (( alias ("int_spindle_is_present"), __visibility__("default")));
void spindle_enable() __attribute__ (( alias ("int_spindle_enable"), __visibility__("default")));
void spindle_disable() __attribute__ (( alias ("int_spindle_disable"), __visibility__("default")));
int spindle_is_enabled() __attribute__ (( alias ("int_spindle_is_enabled"), __visibility__("default")));
#endif

#endif
