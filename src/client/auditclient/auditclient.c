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
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>

unsigned int spindle_la_version(unsigned int version)
{
   return 1;
}

void spindle_la_activity (uintptr_t *cookie, unsigned int flag)
{
   debug_printf3("la_activity(): cookie = %p; flag = %s\n", cookie,
                 (flag == LA_ACT_CONSISTENT) ? "LA_ACT_CONSISTENT" :
                 (flag == LA_ACT_ADD) ?        "LA_ACT_ADD" :
                 (flag == LA_ACT_DELETE) ?     "LA_ACT_DELETE" :
                 "???");
   return;
}

unsigned int spindle_la_objopen(struct link_map *map, Lmid_t lmid, uintptr_t *cookie)
{
   patch_on_linkactivity(map);
   return LA_FLG_BINDTO | LA_FLG_BINDFROM;
}
