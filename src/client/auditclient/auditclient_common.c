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

/* rtld-audit interface functions.  Other rtld-audit functions may be found
   in platform files like auditclient_x86_64.c */


#include "client.h"
#include "auditclient.h"
#include "ldcs_api.h"
#include "intercept.h"
#include "writablegot.h"
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>

static uintptr_t *firstcookie = NULL;
static signed long cookie_shift = 0;

unsigned int la_version(unsigned int version) AUDIT_EXPORT;
char *la_objsearch(const char *name, uintptr_t *cookie, unsigned int flag) AUDIT_EXPORT;
unsigned int la_objopen(struct link_map *map, Lmid_t lmid, uintptr_t *cookie) AUDIT_EXPORT;
void la_activity (uintptr_t *cookie, unsigned int flag) AUDIT_EXPORT;
void la_preinit(uintptr_t *cookie) AUDIT_EXPORT;
unsigned int la_objclose (uintptr_t *cookie) AUDIT_EXPORT;

extern unsigned int spindle_la_version(unsigned int version);
unsigned int la_version(unsigned int version)
{
   int result = client_init();
   if (result == -1)
      return 0;
   debug_printf("la_version function is loaded at %p\n", la_version);
   debug_printf3("la_version(): %d\n", version);
   init_bindings_hash();
   return spindle_la_version(version);
}

char *la_objsearch(const char *name, uintptr_t *cookie, unsigned int flag)
{
   debug_printf3("la_objsearch(): name = %s; cookie = %p; flag = %s\n", name, cookie,
                 (flag == LA_SER_ORIG) ?    "LA_SER_ORIG" :
                 (flag == LA_SER_LIBPATH) ? "LA_SER_LIBPATH" :
                 (flag == LA_SER_RUNPATH) ? "LA_SER_RUNPATH" :
                 (flag == LA_SER_DEFAULT) ? "LA_SER_DEFAULT" :
                 (flag == LA_SER_CONFIG) ?  "LA_SER_CONFIG" :
                 (flag == LA_SER_SECURE) ?  "LA_SER_SECURE" :
                 "???");

   /* check if direct name given --> return name  */
   if (!strchr(name, '/')) {
      debug_printf3("Returning direct name %s after input %s\n", name, name);
      return (char *) name;
   }
   
   return client_library_load(name);
}

extern unsigned int spindle_la_objopen(struct link_map *map, Lmid_t lmid, uintptr_t *cookie);
unsigned int la_objopen(struct link_map *map, Lmid_t lmid, uintptr_t *cookie)
{
   signed long shift;

   /* In glibc the difference between the link_map and the cookie is constant.
      We'll want the link_map later, so the first time we get told about a 
      cookie and link_map we'll calculate and save their difference.  That way 
      we'll be able to shift from cookie to link_map in the future.
      
      If the difference between link_map and cookie becomes non-constant (the
      assert below fails), then we'll have to start storing an actual mapping 
      to convert from cookies to link_maps.
   */
      
      
   debug_printf3("la_objopen(): loading %s, link_map = %p, lmid = %s, cookie = %p\n",
                 map->l_name, map,
                 (lmid == LM_ID_BASE) ?  "LM_ID_BASE" :
                 (lmid == LM_ID_NEWLM) ? "LM_ID_NEWLM" : 
                 "???", 
                 cookie);

   if (!firstcookie) {
      firstcookie = cookie;
   }
   
   shift = ((unsigned char *) map) - ((unsigned char *) cookie);
   if (cookie_shift)
      assert(cookie_shift == shift);
   else {
      cookie_shift = shift;
      debug_printf3("Set cookie_shift to %ld\n", (unsigned long) shift);
   }

   add_wgot_library(map);

   return spindle_la_objopen(map, lmid, cookie);
}

void spindle_la_activity (uintptr_t *cookie, unsigned int flag);
void la_activity (uintptr_t *cookie, unsigned int flag)
{
   debug_printf3("la_activity(): cookie = %p; flag = %s\n", cookie,
                 (flag == LA_ACT_CONSISTENT) ? "LA_ACT_CONSISTENT" :
                 (flag == LA_ACT_ADD) ?        "LA_ACT_ADD" :
                 (flag == LA_ACT_DELETE) ?     "LA_ACT_DELETE" :
                 "???");

   if (flag == LA_ACT_CONSISTENT) {
      remove_libc_rogot();
      mark_newlibs_as_need_writable_got();
   }

   spindle_la_activity(cookie, flag);
   return;
}

struct link_map *get_linkmap_from_cookie(uintptr_t *cookie)
{
   return (struct link_map *) (((unsigned char *) cookie) + cookie_shift);
}

void la_preinit(uintptr_t *cookie)
{
   debug_printf3("la_preinit(): %p\n", cookie);
}

extern unsigned int spindle_la_objclose(uintptr_t *cookie);
unsigned int la_objclose (uintptr_t *cookie)
{
  struct link_map *map;
  debug_printf3("la_objclose() %p\n", cookie);

  map = get_linkmap_from_cookie(cookie);
  rm_wgot_library(map);

  if(cookie == firstcookie) {
     client_done();
  }

  return spindle_la_objclose(cookie);
}

