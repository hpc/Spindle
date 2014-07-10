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

#include "intercept.h"
#include "client.h"
#include "spindle_debug.h"

#include <string.h>
#include <elf.h>

static ElfX_Addr redirect_open(const char *symname, ElfX_Addr value)
{
   if (strcmp(symname, "open") == 0) {
      if (!orig_open)
         orig_open = (void *) value;
      return (ElfX_Addr) rtcache_open;
   }
   else if (strcmp(symname, "open64") == 0) {
      if (!orig_open64)
         orig_open64 = (void *) value;
      return (ElfX_Addr) rtcache_open64;
   }
   else if (strcmp(symname, "fopen") == 0) {
      if (!orig_fopen)
         orig_fopen = (void *) value;
      return (ElfX_Addr) rtcache_fopen;
   }
   else if (strcmp(symname, "fopen64") == 0) {
      if (!orig_fopen64)
         orig_fopen64 = (void *) value;
      return (ElfX_Addr) rtcache_fopen64;
   }
   else
      return (ElfX_Addr) value;
}

static ElfX_Addr redirect_close(const char *symname, ElfX_Addr value)
{
   if (strcmp(symname, "close") == 0) {
      if (!orig_close)
         orig_close = (void *) value;
      return (ElfX_Addr) rtcache_close;
   }
   return value;
}

static ElfX_Addr redirect_stat(const char *symname, ElfX_Addr value)
{
   if (strcmp(symname, "stat") == 0) {
      if (!orig_stat)
         orig_stat = (void *) value;
      return (ElfX_Addr) rtcache_stat;
   }
   else if (strcmp(symname, "lstat") == 0) {
      if (!orig_lstat) 
         orig_lstat = (void *) value;
      return (ElfX_Addr) rtcache_lstat;
   }
   /* glibc internal names */
   else if (strcmp(symname, "__xstat") == 0) {
      if (!orig_xstat)
         orig_xstat = (void *) value;
      return (ElfX_Addr) rtcache_xstat;
   }
   else if (strcmp(symname, "__xstat64") == 0) {
      if (!orig_xstat64)
         orig_xstat64 = (void *) value;
      return (ElfX_Addr) rtcache_xstat64;
   }
   else if (strcmp(symname, "__lxstat") == 0) {
      if (!orig_lxstat)
         orig_lxstat = (void *) value;
      return (ElfX_Addr) rtcache_lxstat;
   }
   else if (strcmp(symname, "__lxstat64") == 0) {
      if (!orig_lxstat64)
         orig_lxstat64 = (void *) value;
      return (ElfX_Addr) rtcache_lxstat64;
   }
   else if (strcmp(symname, "fstat") == 0) {
      if (!orig_fstat)
         orig_fstat = (void *) value;
      return (ElfX_Addr) rtcache_fstat;
   }
   else if (strcmp(symname, "__fxstat") == 0) {
      if (!orig_fxstat)
         orig_fxstat = (void *) value;
      return (ElfX_Addr) rtcache_fxstat;
   }
   else if (strcmp(symname, "__fxstat64") == 0) {
      if (!orig_fxstat64)
         orig_fxstat64 = (void *) value;
      return (ElfX_Addr) rtcache_fxstat64;
   }
   else {
      debug_printf3("Skipped relocation of stat call %s\n", symname);
      return (ElfX_Addr) value;
   }
}

static ElfX_Addr redirect_exec(const char *symname, ElfX_Addr value)
{
   if (strcmp(symname, "execl") == 0) {
      return (ElfX_Addr) execl_wrapper;
   }
   else if (strcmp(symname, "execv") == 0) {
      if (!orig_execv)
         orig_execv = (void *) value;
      return (ElfX_Addr) execv_wrapper;
   }
   else if (strcmp(symname, "execle") == 0) {
      return (ElfX_Addr) execle_wrapper;
   }
   else if (strcmp(symname, "execve") == 0) {
      if (!orig_execve)
         orig_execve = (void *) value;
      return (ElfX_Addr) execve_wrapper;
   }
   else if (strcmp(symname, "execlp") == 0) {
      return (ElfX_Addr) execlp_wrapper;
   }
   else if (strcmp(symname, "execvp") == 0) {
      if (!orig_execvp)
         orig_execvp = (void *) value;
      return (ElfX_Addr) execvp_wrapper;
   }
   else
      return (ElfX_Addr) value;
}

static ElfX_Addr redirect_fork(const char *symname, ElfX_Addr value)
{
   if (strcmp(symname, "vfork") == 0) {
      return (ElfX_Addr) vfork_wrapper;
   }
   else {
      debug_printf3("Not translating fork call %s\n", symname);
      return (ElfX_Addr) value;
   }
}

ElfX_Addr redirect_spindleapi(const char *symname, ElfX_Addr value)
{
   if (strcmp(symname, "spindle_enable") == 0) {
      return (ElfX_Addr) int_spindle_enable;
   }
   else if (strcmp(symname, "spindle_disable") == 0) {
      return (ElfX_Addr) int_spindle_disable;
   }
   else if (strcmp(symname, "spindle_is_enabled") == 0) {
      return (ElfX_Addr) int_spindle_is_enabled;
   }
   else if (strcmp(symname, "spindle_is_present") == 0) {
      return (ElfX_Addr) int_spindle_is_present;
   }
   else if (strcmp(symname, "spindle_open") == 0) {
      return (ElfX_Addr) int_spindle_open;
   }
   else if (strcmp(symname, "spindle_stat") == 0) {
      return (ElfX_Addr) int_spindle_stat;
   }
   else if (strcmp(symname, "spindle_lstat") == 0) {
      return (ElfX_Addr) int_spindle_lstat;
   }
   else if (strcmp(symname, "spindle_fopen") == 0) {
      return (ElfX_Addr) int_spindle_fopen;
   }
   else {
      return value;
   }
}

ElfX_Addr client_call_binding(const char *symname, ElfX_Addr symvalue)
{
   if (run_tests && strcmp(symname, "spindle_test_log_msg") == 0)
      return (Elf64_Addr) spindle_test_log_msg;
   else if (strncmp("spindle_", symname, 8) == 0)
      return redirect_spindleapi(symname, symvalue);
   else if (intercept_open && strstr(symname, "open"))
      return redirect_open(symname, symvalue);
   else if (intercept_exec && strstr(symname, "exec")) 
      return redirect_exec(symname, symvalue);
   else if (intercept_stat && strstr(symname, "stat"))
      return redirect_stat(symname, symvalue);
   else if (intercept_close && strcmp(symname, "close") == 0)
      return redirect_close(symname, symvalue);
   else if (intercept_fork && strstr(symname, "fork"))
      return redirect_fork(symname, symvalue);
   else if (!app_errno_location && strcmp(symname, ERRNO_NAME) == 0) {
      app_errno_location = (errno_location_t) symvalue;
      return symvalue;
   }
   else
      return symvalue;
}

