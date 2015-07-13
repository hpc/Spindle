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

#ifndef LDCS_HASH_H
#define LDCS_HASH_H

#define HASH_SIZE (10*1024)
typedef unsigned ldcs_hash_key_t;

typedef enum {
   HASH_ENTRY_STATUS_USED,
   HASH_ENTRY_STATUS_NEW,
   HASH_ENTRY_STATUS_FREE,
   HASH_ENTRY_STATUS_UNKNOWN
} ldcs_hash_entry_status_t;

struct ldcs_hash_entry_t
{
  ldcs_hash_entry_status_t  state;
  int   ostate;
  char *dirname;
  char *filename;
  char *localpath;
  void *buffer;
  size_t buffer_size;
  ldcs_hash_key_t hash_val;
  struct ldcs_hash_entry_t *next;
  struct ldcs_hash_entry_t *dir_next;
};

int ldcs_hash_init();
ldcs_hash_key_t ldcs_hash_Val(const char *str);
void ldcs_hash_addEntry(char *dirname, char *filename);

struct ldcs_hash_entry_t *ldcs_hash_updateEntryOState(char *filename, char *dirname, int ostate);
struct ldcs_hash_entry_t *ldcs_hash_updateEntry(char *filename, char *dirname, char *localname, 
                                                void *buffer, size_t buffer_size);

struct ldcs_hash_entry_t *ldcs_hash_Lookup(const char *filename);
struct ldcs_hash_entry_t *ldcs_hash_Lookup_FN_and_DIR(const char *filename, const char *dirname);

void ldcs_hash_dump(char *tofile);

struct ldcs_hash_entry_t *ldcs_hash_getFirstEntryForDir(char *dirname);
struct ldcs_hash_entry_t *ldcs_hash_getNextEntryForDir(struct ldcs_hash_entry_t *prev_entry);
#endif
