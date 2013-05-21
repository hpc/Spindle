/*
This file is part of Spindle.  For copyright information see the COPYRIGHT 
file in the top level directory, or at 
<TODO:URL>.

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

#include <cstdio>
#include <cstring>
#include <set>
#include <string>

extern "C" {
#include "ldcs_cache.h"
}

using namespace std;

#define STR2(X) #X
#deifne STR(X) STR2(X)

bool parsePreloadFile(string filename)
{
   char pathname[MAX_PATH_LEN+1], cwd[MAX_FILE_LEN+1], dir[MAX_FILE_LEN+1], file[MAX_FILE_LEN+1];

   set<string> all_dirs, all_files;

   FILE *f = fopen(filename.c_str(), "r");
   if (!f) {
      fprintf(stderr, "Error opening preload file %s: %s\n", filename.c_str(), strerror(errno));
      return false;
   }

   getcwd(cwd, MAX_FILE_LEN+1);
   cwd[MAX_FILE_LEN] = '\0';

   for (;;) {
      int result = fscanf(f, "%" STR(MAX_PATH_LEN) "s", pathname);
      if (result == EOF)
         break;
      pathname[MAX_PATH_LEN] = '\0';
      
      parseFilenameNoAlloc(pathname, file, dir, MAX_PATH_LEN);
      file[MAX_PATH_LEN] = '\0';
      dir[MAX_PATH_LEN] = '\0';
      addCWDToDir(cwd, dir, MAX_PATH_LEN);
      reducePath(dir);
   
      all_dirs.insert(string(dir));
      all_files.insert(string(file));      
   }

   size_t size = 0;
   size += 4; //Num dirs as int
   size += 4; //Num files as int
   for (set<string>::iterator i = all_dirs.begin(); i != all_dirs.end(); i++)
      size += i->length() + 1; //String + 0-terminated character
   for (set<string>::iterator i = all_files.begin(); i != all_files.end(); i++)
      size += i->length() + 1; //String + 0-terminated character

   char *buffer = (char *) malloc(size);
   assert(buffer);
   
   size_t cur = 0;
   *((int *) (buffer+cur)) = (int) all_dirs.size();
   cur += sizeof(int);
   *((int *) (buffer+cur)) = (int) all_files.size();
   cur += sizeof(int);

   for (set<string>::iterator i = all_dirs.begin(); i != all_dirs.end(); i++) {
      int length = i->length + 1;
      memcpy(buffer + cur, i->c_str(), length);
      cur += length;
   }
   for (set<string>::iterator i = all_files.begin(); i != all_files.end(); i++) {
      int length = i->length + 1;
      memcpy(buffer + cur, i->c_str(), length);
      cur += length;
   }

   return true;
}
