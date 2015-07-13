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

#include <assert.h>
#include <stdlib.h>

typedef int (*func_t)(void);
typedef void (*cb_func_t)(func_t, char *);

struct func_table_t {
   func_t function;
   char *name;
};

cb_func_t cb = NULL;
#define MAX_LIBRARIES 1024
struct func_table_t func_table[MAX_LIBRARIES];
int cur_library = 0;

void setup_func_callback(cb_func_t c)
{
   int i;
   cb = c;

   for (i = 0; i<cur_library; i++) {
      cb(func_table[i].function, func_table[i].name);
   }
}

void register_calc_function(func_t f, char *n)
{
   if (cb) {
      cb(f, n);
      return;
   }

   assert(cur_library != MAX_LIBRARIES);
   func_table[cur_library].function = f;
   func_table[cur_library].name = n;
   cur_library++;
}

