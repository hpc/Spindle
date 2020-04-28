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

#if !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include <dlfcn.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#if !defined(SO_NAME)
#error SO_NAME not defined
#endif

#define STR2(x) #x
#define STR(x) STR2(x)

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(FUNC_NAME)
   
extern int FUNC_NAME();

extern void register_calc_function(int (*func)(void), char *);

static void onload() __attribute__((constructor));
static void onload()
{
   register_calc_function(FUNC_NAME, (char *) STR(SO_NAME));
}

#endif

typedef struct libs_list_t {
   const char *libname;
   int count;
   struct libs_list_t *next;
} libs_list_t;

libs_list_t *master_liblist_head = NULL;
int liblist_error_code;
   
static void add_library() __attribute__((constructor));
static void add_library()
{
   libs_list_t **my_head, *i, *newnode;
   char *errmsg;
   int *my_error_code;

   dlerror();
   my_head = (libs_list_t **) dlsym(RTLD_DEFAULT, "master_liblist_head");
   errmsg = dlerror();
   if (!my_head && errmsg) {
      fprintf(stderr, "ERROR: Could not load library head list.  Aborting test early\n");
      exit(-1);
   }

   for (i = *my_head; i != NULL; i = i->next) {
      if (strcmp(i->libname, STR(SO_NAME)) != 0)
         continue;
      
      i->count++;
      fprintf(stderr, "ERROR: %s loaded %d times\n", i->libname, i->count);
      dlerror();
      my_error_code = (int *) dlsym(RTLD_DEFAULT, "liblist_error_code");
      errmsg = dlerror();
      if (!my_error_code && errmsg) {
         fprintf(stderr, "ERROR: Could not find liblist_error_code.  Aborting test early\n");
         exit(-1);
      }
      (*my_error_code)++;
      return;
   }

   newnode = (libs_list_t *) malloc(sizeof(libs_list_t));
   newnode->libname = STR(SO_NAME);
   newnode->count = 1;
   newnode->next = *my_head;
   *my_head = newnode;
}

int get_liblist_error()
{
   int *my_error_code;
   char *errmsg;
   
   dlerror();
   my_error_code = (int *) dlsym(RTLD_DEFAULT, "liblist_error_code");
   errmsg = dlerror();
   if (!my_error_code && errmsg) {
      fprintf(stderr, "ERROR: Could not find liblist_error_code.  Aborting test early\n");
      exit(-1);
   }

   return *my_error_code;
}
   
#if defined(__cplusplus)
}
#endif

