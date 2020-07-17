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

#include <map>
#include <string>
#include <cassert>
#include <utility>
#include "spindle_debug.h"
#include "stat_cache.h"

static std::map<std::string, char*> stat_table;
static std::map<std::string, char*> lstat_table;
static std::map<std::string, char*> ldso_table;

int init_stat_cache()
{
   return 0;
}

void add_stat_cache(char *pathname, char *data, metadata_t stattype)
{
   if (stattype == metadata_loader)
      debug_printf3("Adding ldso cache entry %s = %s\n", pathname, data ? data : "NULL");
   else
      debug_printf3("Adding %sstat cache entry %s = %s\n",
                    (stattype == metadata_lstat) ? "l" : "",
                    pathname, data ? data : "NULL");

   std::string pathname_key = pathname;
   std::map<std::string, char*> *table;
   switch (stattype) {
      case metadata_none: assert(0); break;
      case metadata_stat: table = &stat_table; break;
      case metadata_lstat: table = &lstat_table; break;         
      case metadata_loader: table = &ldso_table; break;         
   }

   table->insert(std::make_pair(pathname_key, data));
}

int lookup_stat_cache(char *pathname, char **data, metadata_t stattype)
{
   std::string pathname_key = pathname;

   std::map<std::string, char*> *table;
   switch (stattype) {
      case metadata_none: assert(0); break;      
      case metadata_stat: table = &stat_table; break;
      case metadata_lstat: table = &lstat_table; break;
      case metadata_loader: table = &ldso_table; break;
   }

   std::map<std::string, char*>::iterator i = table->find(pathname_key);
   if (i == table->end()) {
      debug_printf3("Looked up metadata cache entry %s, not cached\n", pathname);
      *data = NULL;
      return -1;
   }

   *data = i->second;
   return 0;
}
