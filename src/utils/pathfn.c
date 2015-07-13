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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "ldcs_api.h"
#include "ldcs_cache.h"

int addCWDToDir(const char *cwd, char *dir, int result_size)
{
   int cwd_len, dir_len, i;
   if (dir[0] == '/')
      return 0;
   cwd_len = strlen(cwd);
   dir_len = strlen(dir);
   if (!cwd_len)
      return 0;
   if (cwd[cwd_len-1] == '/')
      cwd_len = cwd_len - 1;
   if (dir_len + cwd_len + 1 >= result_size)
      return -1;

   if (dir[0] == '\0') {
      strncpy(dir, cwd, cwd_len);
      dir[cwd_len] = '\0';
      return 0;
   }

   for (i = dir_len; i >= 0; i--)
      dir[i + cwd_len + 1] = dir[i];
   dir[cwd_len] = '/';
   strncpy(dir, cwd, cwd_len);
   return 0;
}

int parseFilenameNoAlloc(const char *name, char *file, char *dir, int result_size)
{
   char *last_slash = strrchr(name, '/');
   int size;
   if (last_slash) {
      strncpy(file, last_slash+1, result_size);
      file[result_size-1] = '\0';
      size = last_slash - name;
      if (size >= result_size)
         size = result_size-1;
      strncpy(dir, name, size);
      dir[size] = '\0'; 
   }
   else {
      strncpy(file, name, result_size);
      file[result_size-1] = '\0';
      dir[0] = '\0';
   }
   return 0;
}

/* Remove '.', '..', '//' from directory strings to normalize name.  We don't
   use the glibc functions that do this because they might access disk, which 
   isn't appropriate for Spindle.  Note that this means we can't resolve symlinks
   to a normalized name here. */
int reducePath(char *dir)
{
   int slash_begin = 0, slash_end, i, tmpdir_loc = 0;
   int dir_len = strlen(dir);
   char tmpdir[MAX_PATH_LEN+1];

   while (dir[slash_begin]) {
      slash_end = slash_begin+1;
      while (dir[slash_end] != '\0' && dir[slash_end] != '/') slash_end++;

      if (slash_end == slash_begin + 1) {
         /* / case.  Do nothing, we will just advance past the directory */
      }
      else if (dir[slash_begin+1] == '.' && slash_end == slash_begin + 2) {
         /* /./ case.  Do nothing, we will just advance past the directory */
      }
      else if (dir[slash_begin+1] == '.' && dir[slash_begin+2] == '.' && slash_end == slash_begin + 3) {
         /* /../ case.  Back up tmpdir one directory */
         if (tmpdir_loc == 0) {
            return -1;
         }
         while (tmpdir[tmpdir_loc] != '/' && tmpdir_loc != 0)
            tmpdir_loc--;
         tmpdir[tmpdir_loc] = '\0';
      }
      else {
         /* Normal directory.  Copy from dir to dir */
         for (i = slash_begin; i != slash_end; i++, tmpdir_loc++) {
            tmpdir[tmpdir_loc] = dir[i];
         }
         tmpdir[tmpdir_loc] = '\0';
      }
      slash_begin = slash_end;
   }
   strncpy(dir, tmpdir, dir_len);
   return 0;
}

char *concatStrings(const char *str1, int str1_len, const char *str2, int str2_len) {
   char *buffer = NULL;
   unsigned cur_size = str1_len + str2_len + 1;

   buffer = (char *) malloc(cur_size);
   strncpy(buffer, str1, str1_len);
   if (str2)
      strncpy(buffer+str1_len, str2, str2_len);
   buffer[str1_len+str2_len] = '\0';

   return buffer;
}

