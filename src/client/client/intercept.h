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

#if !defined(INTERCEPT_H_)
#define INTERCEPT_H_

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>

extern int (*orig_stat)(const char *path, struct stat *buf);
extern int (*orig_lstat)(const char *path, struct stat *buf);
extern int (*orig_xstat)(int vers, const char *path, struct stat *buf);
extern int (*orig_lxstat)(int vers, const char *path, struct stat *buf);
extern int (*orig_xstat64)(int vers, const char *path, struct stat *buf);
extern int (*orig_lxstat64)(int vers, const char *path, struct stat *buf);
extern int (*orig_fstat)(int fd, struct stat *buf);
extern int (*orig_fxstat)(int vers, int fd, struct stat *buf);
extern int (*orig_fxstat64)(int vers, int fd, struct stat *buf);
extern int (*orig_execv)(const char *path, char *const argv[]);
extern int (*orig_execve)(const char *path, char *const argv[], char *const envp[]);
extern int (*orig_execvp)(const char *file, char *const argv[]);
extern pid_t (*orig_fork)();
extern int (*orig_open)(const char *pathname, int flags, ...);
extern int (*orig_open64)(const char *pathname, int flags, ...);
extern FILE* (*orig_fopen)(const char *pathname, const char *mode);
extern FILE* (*orig_fopen64)(const char *pathname, const char *mode);
extern int (*orig_close)(int fd);

int rtcache_stat(const char *path, struct stat *buf);
int rtcache_lstat(const char *path, struct stat *buf);
int rtcache_xstat(int vers, const char *path, struct stat *buf);
int rtcache_xstat64(int vers, const char *path, struct stat *buf);
int rtcache_lxstat(int vers, const char *path, struct stat *buf);
int rtcache_lxstat64(int vers, const char *path, struct stat *buf);
int rtcache_fstat(int fd, struct stat *buf);
int rtcache_fxstat(int vers, int fd, struct stat *buf);
int rtcache_fxstat64(int vers, int fd, struct stat *buf);

int rtcache_open(const char *path, int oflag, ...);
int rtcache_open64(const char *path, int oflag, ...);
FILE *rtcache_fopen(const char *path, const char *mode);
FILE *rtcache_fopen64(const char *path, const char *mode);
int rtcache_close(int fd);

int execl_wrapper(const char *path, const char *arg0, ...);
int execv_wrapper(const char *path, char *const argv[]);
int execle_wrapper(const char *path, const char *arg0, ...);
int execve_wrapper(const char *path, char *const argv[], char *const envp[]);
int execlp_wrapper(const char *path, const char *arg0, ...);
int execvp_wrapper(const char *path, char *const argv[]);
pid_t vfork_wrapper();

int int_spindle_open(const char *pathname, int flags, ...);
FILE *int_spindle_fopen(const char *path, const char *opts);
int int_spindle_stat(const char *path, struct stat *buf);
int int_spindle_lstat(const char *path, struct stat *buf);
int int_spindle_lstat(const char *path, struct stat *buf);
int int_spindle_is_present();
void int_spindle_enable();
void int_spindle_disable();
int int_spindle_is_enabled();
void int_spindle_test_log_msg(char *buffer);

struct spindle_binding_t {
   const char *name;
   void **libc_func;
   const char *spindle_name;
   void *spindle_func;
};

void init_bindings_hash();
struct spindle_binding_t *lookup_in_binding_hash(const char *name);
struct spindle_binding_t *get_bindings();

#endif
