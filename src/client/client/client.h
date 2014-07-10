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

#if !defined(_CLIENT_H_)
#define _CLIENT_H_

#define _GNU_SOURCE
#include <sys/stat.h>
#include <assert.h>
#include <stdio.h>

#define NOT_FOUND_PREFIX "/__not_exists"

extern int use_ldcs;
extern int ldcsid;

typedef unsigned long ElfX_Addr;

/**
 * These functions are called by the audit hooks to do the major
 * pieces of work.
 **/
ElfX_Addr client_call_binding(const char *symname, ElfX_Addr symvalue);
char *client_library_load(const char *libname);
int client_init();
int client_done();

/**
 * Helper functions used throughout the client
 **/
void set_errno(int newerrno);
void patch_on_load_success(const char *rewritten_name, const char *orig_name);
void sync_cwd();
void check_for_fork();

/**
 * Worker functions for intercepting open and stat calls
 **/
#define IS_64    (1 << 0)
#define IS_LSTAT (1 << 1)
#define IS_XSTAT (1 << 2)
#define ORIG_STAT -2
int handle_stat(const char *path, struct stat *buf, int flags);
int open_worker(const char *path, int oflag, mode_t mode, int is_64);
FILE *fopen_worker(const char *path, const char *mode, int is_64);
void remap_executable();

int get_relocated_file(int fd, const char *name, char** newname);
int get_stat_result(int fd, const char *path, int is_lstat, int *exists, struct stat *buf);
int get_existance_test(int fd, const char *path, int *exists);

/**
 * Tracking python prefixes
 **/
typedef struct {
   char *path;
   int pathsize;
} python_path_t;
extern python_path_t *pythonprefixes;
void parse_python_prefixes(int fd);

void test_log(const char *name);

extern unsigned long opts;

extern int intercept_open;
extern int intercept_exec;
extern int intercept_stat;
extern int intercept_close;
extern int intercept_fork;
void spindle_test_log_msg(char *buffer);

/* ERRNO_NAME currently refers to a glibc internal symbol. */
#define ERRNO_NAME "__errno_location"
typedef int *(*errno_location_t)(void);
extern errno_location_t app_errno_location;

#endif
