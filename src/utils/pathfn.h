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

#if !defined(PATHFN_H_)
#define PATHFN_H_

#include <unistd.h>

#if defined(__cplusplus)
extern "C" {
#endif

int parseFilenameNoAlloc(const char *name, char *file, char *dir, int result_size);
int addCWDToDir(pid_t pid, char *dir, int result_size);
int reducePath(char *dir);
char *concatStrings(const char *str1, int str1_len, const char *str2, int str2_len);

#if defined(__cplusplus)
}
#endif

#endif
