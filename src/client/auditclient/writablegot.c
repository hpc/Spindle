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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <link.h>
#include <unistd.h>
#include <sys/mman.h>
#include "spindle_debug.h"
#include "config.h"

struct got_range_t {
   unsigned long start;
   unsigned long end;
   struct link_map *map;
   int is_marked_writable;
   int is_new;
};

/**
 * We'll maintain a sorted array of GOT regions that have been marked writable.
 *
 * We'll have to add/remove library GOT ranges as libraries are loaded/unloaded,
 * but lookup of GOT ranges should be quick.
 **/
static struct got_range_t *gots;
static signed int gots_last_entry;
static signed int gots_size;
#define GOTS_INITIAL_SIZE 32

#if defined(arch_x86_64)
#define DEFAULT_RELENTSZ 24
#define EXTRA_GOT_ENTRIES 3
#elif defined(arch_ppc64) || defined(arch_ppc64le)
#define DEFAULT_RELENTSZ 24
#define EXTRA_GOT_ENTRIES 2
#elif defined(arch_aarch64)
#define DEFAULT_RELENTSZ 24
#define EXTRA_GOT_ENTRIES 4
#else
#error Need to fill in got info
#endif

int add_wgot_library(struct link_map *map)
{
   ElfW(Dyn) *dynamic_section;
   void *got_table = NULL;
   unsigned long rel_size = 0, relent_size = 0, relcount, gotsize, start, end;
   signed int i;
   struct got_range_t *tmprange;
   

   //Allocate or grow the gots array if needed.
   if (!gots) {
      gots_size = GOTS_INITIAL_SIZE;
      gots = (struct got_range_t *) malloc(sizeof(struct got_range_t) * GOTS_INITIAL_SIZE);
      memset(gots, 0, sizeof(struct got_range_t) * GOTS_INITIAL_SIZE);
   }
   if (gots_last_entry+1 == gots_size) {
      gots_size *= 2;
      tmprange = (struct got_range_t *) realloc(gots, sizeof(struct got_range_t) * gots_size);
      if (!tmprange) {
         err_printf("Could not allocate memory for gots_range table of size %lu\n", sizeof(struct got_range_t) * gots_size);
         return -1;
      }
      gots = tmprange;
   }

   //For the current library, map, lookup the range of its GOT table.
   dynamic_section = map->l_ld;
   if (!dynamic_section) {
      err_printf("Library %s does not have a dynamic section\n", map->l_name ? map->l_name : "[NO NAME]");
      return -1;
   }
   for (; dynamic_section->d_tag != DT_NULL; dynamic_section++) {
      if (dynamic_section->d_tag == DT_PLTGOT) {
         got_table = (void*) dynamic_section->d_un.d_ptr;
      }
      if (dynamic_section->d_tag == DT_PLTRELSZ) {
         rel_size = (unsigned long) dynamic_section->d_un.d_val;
      }
      if (dynamic_section->d_tag == DT_RELAENT) {
         relent_size = (unsigned long) dynamic_section->d_un.d_val;
      }
      if (got_table && rel_size && relent_size) {
         break;
      }
   }

   if (!got_table) {
      //Could have a library that doesn't call outside of itself and doesn't have PLT GOTs.
      // unlikely, but possible.
      debug_printf("Warning: Library %s does not appear to have DT_JMPREL (%p) or DT_PLTRELSZ (%lu)\n",
                   map->l_name ? map->l_name : "[NO NAME]", got_table, rel_size);
      return -1;
   }

   if (!relent_size) {
      relent_size = DEFAULT_RELENTSZ;
   }
   relcount = rel_size / relent_size;
   gotsize = (relcount + EXTRA_GOT_ENTRIES + 1) * sizeof(void*);

   start = (unsigned long) got_table;
   end = start + gotsize;

   //Sorted-insert the info for the new library into the gots array.
   for (i = gots_last_entry-1; i >= 0; i--) {
      if (gots[i].start > start) {
         gots[i+1] = gots[i];
      }
      if (gots[i].start == start) {
         err_printf("Adding GOT range that already exists for library %s with range %lx to %lx over "
                    "library %s with range %lx to %lx\n",
                    map->l_name ? map->l_name : "[NO NAME]", start, end,
                    gots[i].map->l_name ? gots[i].map->l_name : "[NO NAME]", gots[i].start, gots[i].end);
         return -1;

      }
      if (gots[i].start < start) {
         break;
      }
   }
   gots[i+1].start = start;
   gots[i+1].end = end;
   gots[i+1].map = map;
   gots[i+1].is_marked_writable = 0;
   gots[i+1].is_new = 1;
   gots_last_entry++;
   debug_printf2("Inserted library %s with GOT range %lx to %lx into the tracked got ranges array\n",
                 map->l_name ? map->l_name : "[NO NAME]", start, end);

   return 0;
}

int rm_wgot_library(struct link_map *map)
{
   signed int i;
   int seen_map = 0;

   //Sorted array delete
   for (i = 0; i < gots_last_entry; i++) {
      if (gots[i].map == map)
         seen_map = 1;
      if (seen_map && (i+1 < gots_last_entry))
         gots[i] = gots[i+1];
   }
   if (seen_map) {
      debug_printf2("Deleted library %s from GOT range\n", map->l_name ? map->l_name : "[NO NAME]");
      gots_last_entry--;
      return 0;
   }
   else {
      err_printf("Did not find library %s when deleting from GOT range\n", map->l_name ? map->l_name : "[NO NAME]");
      return -1;
   }
}

static int markw(struct got_range_t *got)
{
   unsigned long pagestart, pagesize;
   int result;
   
   if (got->is_marked_writable) {
      debug_printf3("GOT for %s is already writable, not changing permissions\n", got->map->l_name ? got->map->l_name : "[NO NAME]");
      return 0;
   }

   debug_printf2("Setting permissions to make GOT for %s writable\n", got->map->l_name ? got->map->l_name : "[NO NAME]");
   pagesize = (unsigned long) getpagesize();
   pagestart = got->start & ~(pagesize-1);

   result = mprotect((void*) pagestart, got->end - pagestart, PROT_READ | PROT_WRITE);
   if (result == -1) {
      err_printf("Could not mark GOT region %lx to %lx as writable\n", pagestart, got->end);
      return -1;
   }
   got->is_marked_writable = 1;
   return 0;
}

int make_got_writable(void *got_entry, struct link_map *map)
{
   //This function could be invoked on multiple threads.  It should be thread safe against itself.
   // Modification to the above gots list shouldn't be concurrent to this, as glibc locks down while
   // load/unloading libraries.
   unsigned long got_entry_address = (unsigned long) got_entry;
   signed int hi, low, cur, prev_cur = -1;

   //Bisect search for got based on entry address, then mark it writable.
   got_entry_address = (unsigned long) got_entry;
   low = 0;
   hi = gots_last_entry;
   while (low != hi) {
      cur = (hi + low) / 2;

      if (cur == prev_cur || cur == gots_last_entry)
         break;
      else if (got_entry_address >= gots[cur].start && got_entry_address < gots[cur].end)
         return markw(gots + cur);
      else if (got_entry_address > gots[cur].start)
         low = cur;
      else if (got_entry_address < gots[cur].start)
         hi = cur;
      prev_cur = cur;
   }
   
   err_printf("Could not find current library %s with got %lx in got list\n", map->l_name ? map->l_name : "[NO NAME]", got_entry_address);
   return -1;
}

void mark_newlibs_as_need_writable_got()
{
   signed int i;
   for (i = 0; i < gots_last_entry; i++) {
      if (gots[i].is_new) {
         gots[i].is_new = 0;
         gots[i].is_marked_writable = 0;
      }
   }
   
}
