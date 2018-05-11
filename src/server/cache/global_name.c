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

#define INITIAL_GLOBAL_NAME_ARRAY_SIZE 1
typedef struct global_name_entry_t
{
   char *global_name;
} global_name_entry_t;

static global_name_entry_t *global_name_array;
static size_t current_global_name_array_size = INITIAL_GLOBAL_NAME_ARRAY_SIZE;
static size_t hiwat_global_name_array_size;
static size_t global_name_array_index;

int init_global_name_list()
{
  global_name_array_index = 0;
  current_global_name_array_size = INITIAL_GLOBAL_NAME_ARRAY_SIZE;
  hiwat_global_name_array_size = (current_global_name_array_size * 8) / 10;
  global_name_array = (global_name_entry_t *)malloc(sizeof(global_name_entry_t) * current_global_name_array_size);
  assert(global_name_array);

  return 0;
}

void grow_global_name_list()
{
  current_global_name_array_size *= 2;
  hiwat_global_name_array_size = (current_global_name_array_size * 8) / 10;
  global_name_array = (global_name_entry_t *)realloc(global_name_array, sizeof(global_name_entry_t) * current_global_name_array_size);
  assert(global_name_array);
}

void add_global_name(char* filename, char* dirname, char* localpath) 
{
  char path[MAX_PATH_LEN];

  sprintf(path, "%s/%s", dirname, filename);

  debug_printf3("%s Adding %s, %s, index=%ld\n", __func__, localpath, path, global_name_array_index);

  if (global_name_array_index > hiwat_global_name_array_size)
    grow_global_name_list();

  global_name_array[global_name_array_index++].global_name = strdup(path);
}

static int get_global_file_index(char* file)
{
  char buf[MAX_PATH_LEN];
  int len = strlen(file);
  int cnt;
  int rval;
  int val;

  assert("Filename length too large" && len < MAX_PATH_LEN);
  
  for (cnt = 0; cnt < len; ++cnt) {
    if (file[cnt] == '_' && cnt > 0)
      break;

    if ( ! isxdigit(file[cnt]) )
      return -1;

    buf[cnt] = file[cnt];
  }
  if (cnt == 0)
    return -1;

  buf[cnt] = '\0';

  rval = sscanf(buf, "%x", &val);
  assert(rval == 1);

  return val;
}

char* lookup_global_name(char* localpath)
{
  char *fname = ldcs_is_a_localfile(localpath);
  int index;

  if (fname == NULL) {
    debug_printf3("%s Returning early since %s is not a local name\n", __func__, localpath);
    return NULL;      /* Not a local name */
  }

  index = get_global_file_index(fname);
  assert(index < global_name_array_index);

  debug_printf3("%s global name %s for local name %s found at index %d\n", __func__, global_name_array[index].global_name, localpath, index);
  return global_name_array[index].global_name;
}
