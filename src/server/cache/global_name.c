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

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include "ldcs_audit_server_filemngt.h"
#include "spindle_debug.h"
#include "global_name.h"

#define INITIAL_GLOBAL_NAME_ARRAY_SIZE (10*1024)
typedef struct global_name_entry_t
{
   char *global_name;
} global_name_entry_t;

static global_name_entry_t *global_name_array;
static ssize_t current_global_name_array_size = INITIAL_GLOBAL_NAME_ARRAY_SIZE;
static ssize_t hiwat_global_name_array_size;
static ssize_t global_name_array_max_index;

int init_global_name_list()
{
  int i;
  global_name_array_max_index = 0;
  current_global_name_array_size = INITIAL_GLOBAL_NAME_ARRAY_SIZE;
  hiwat_global_name_array_size = (current_global_name_array_size * 8) / 10;
  global_name_array = (global_name_entry_t *)malloc(sizeof(global_name_entry_t) * current_global_name_array_size);
  assert(global_name_array);
  for (i = 0; i < current_global_name_array_size; i++)
     global_name_array[i].global_name = NULL;

  return 0;
}

void grow_global_name_list()
{
  int newsize, i;
  newsize = current_global_name_array_size * 2;
  hiwat_global_name_array_size = (newsize * 8) / 10;
  global_name_array = (global_name_entry_t *)realloc(global_name_array, sizeof(global_name_entry_t) * newsize);
  assert(global_name_array);
  for (i = current_global_name_array_size; i < newsize; i++)
     global_name_array[i].global_name = NULL;
  current_global_name_array_size = newsize;
}

static int get_global_file_index(char* file)
{
  char buf[MAX_PATH_LEN+1];
  int len = strlen(file);
  int cnt;
  int rval;
  int val;

  if (len >= sizeof(buf)) {
     err_printf("Filename %s too large\n", file);
     return -1;
  }
  
  for (cnt = 0; cnt < len; ++cnt) {
    if (file[cnt] == '-' && cnt > 0)
      break;

    if ( ! isxdigit(file[cnt]) ) {
      return -1;
    }

    buf[cnt] = file[cnt];
  }
  if (cnt == 0)
    return -1;

  buf[cnt] = '\0';

  rval = sscanf(buf, "%x", &val);
  assert(rval == 1);

  return val;
}

static void parse_name(char* localpath, char **dpart, char **fpart, int *index_val)
{
  char *dname, *fname, *dash;
  int index;

  *index_val = -1;
  *fpart = NULL;
  *dpart = NULL;

  dname = ldcs_is_a_localfile(localpath);
  if (dname == NULL) {
     return;
  }

  if (dname[0] != '/')
     dname--;
  
  fname = strrchr(dname, '/');
  if (!fname)
     fname = dname;
  else 
     fname = fname + 1;
  *dpart = dname;
  *fpart = fname;

  dash = strchr(fname, '-');
  if (dash == NULL) {
     return;
  }

  if (strncmp(dash, "-spindlens-", 11) != 0) {
     return;
  }
  
  index = get_global_file_index(fname);
  if (index < 0) {
     debug_printf3("WARNING: Spindle prefix present, but could not parse index for %s: %d", localpath, index);
     return;
  }
  if (index > 10*current_global_name_array_size) {
     //Not a reasonable jump in size.  Probably a bug.
     debug_printf("WARNING: Got unreasonable size growth in index.  Something's probably wrong. %d >> %ld\n",
                  index, current_global_name_array_size);
     return;
  }

  *index_val = index;
  return;
}

void add_global_name(char* pathname, char* localpath) 
{
  char *dname, *fname;
  int index;

  parse_name(localpath, &dname, &fname, &index);
  if (index == -1) {
     err_printf("Given name %s that couldn't be parsed\n", localpath);
     return;
  }
  debug_printf3("Adding %s, %s, index=%d\n", localpath, pathname, index);

  while (index > hiwat_global_name_array_size)
    grow_global_name_list();

  if (index >= global_name_array_max_index)
     global_name_array_max_index = index;

  if (global_name_array[index].global_name) {
     debug_printf("WARNING: global name map already had entry for %s at index %d: %s\n",
                  localpath, index, pathname);
     return;
  }
  global_name_array[index].global_name = strdup(pathname);
}

char* lookup_global_name(char* localpath)
{
   static char *replacement_path = NULL;
   char *dname, *fname, *result;
   int index;

   parse_name(localpath, &dname, &fname, &index);
   if (!dname) {
      //Not a local path;
      return NULL;
   }

   if (index >= 0 && index <= global_name_array_max_index) {
      result = global_name_array[index].global_name;
      if (result) {
         debug_printf2("Translating request for local path %s using global_name cache\n", localpath);
         return result;
      }
   }

   debug_printf2("Translating request for local path %s by reconstructing orignal name\n", localpath);
   if (!replacement_path) {
      replacement_path = (char *) malloc(MAX_PATH_LEN+1);
   }
   strncpy(replacement_path, dname, MAX_PATH_LEN+1);
   replacement_path[MAX_PATH_LEN] = '\0';
   return replacement_path;
}
