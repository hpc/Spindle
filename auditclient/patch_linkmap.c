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


#define _GNU_SOURCE

#include <elf.h>
#include <link.h>
#include <stdlib.h>
#include <string.h>

#include "client.h"
#include "auditclient.h"
#include "client_heap.h"
#include "client_api.h"
#include "spindle_launch.h"

static char *last_rewritten_name = NULL;
static char *last_orig_name = NULL;

void patch_on_load_success(const char *rewritten_name, const char *orig_name)
{
   if (!(opts & OPT_DEBUG)) {
      return;
   }

   if (last_orig_name)
      spindle_free(last_orig_name);

   last_rewritten_name = (char *) rewritten_name;
   last_orig_name = spindle_strdup(orig_name);
   debug_printf2("Patching link map %s -> %s\n", rewritten_name, orig_name);
}

void patch_on_linkactivity(struct link_map *lmap)
{
   if (!(opts & OPT_DEBUG))
      return;
   if (!last_rewritten_name || !last_orig_name)
      return;

   if (lmap->l_name == last_rewritten_name || strcmp(lmap->l_name, last_rewritten_name) == 0) {
      lmap->l_name = last_orig_name;
   }
   else {
      spindle_free(last_orig_name);
   }
   last_rewritten_name = NULL;
   last_orig_name = NULL;
}
