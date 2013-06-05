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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <cassert>

#include "spindle_usrdata.h"
#include "spindle_debug.h"

extern "C" {
#include "ldcs_api_opts.h"
}

template<typename T>
void pack_param(T value, char *buffer, int &pos)
{
   memcpy(buffer + pos, &value, sizeof(T));
   pos += sizeof(T);
}

template<>
void pack_param<char*>(char *value, char *buffer, int &pos)
{
   if (value == NULL) {
      value = const_cast<char*>("");
   }
   unsigned int strsize = strlen(value) + 1;
   memcpy(buffer + pos, value, strsize);
   pos += strsize;
}

template<typename T>
void unpack_param(T &value, char *buffer, int &pos)
{
   memcpy(&value, buffer + pos, sizeof(T));
   pos += sizeof(T);
}

template<>
void unpack_param<char*>(char* &value, char *buffer, int &pos)
{
   unsigned int strsize = strlen(buffer + pos) + 1;
   value = (char *) malloc(strsize);
   strncpy(value, buffer + pos, strsize);
   pos += strsize;
}

int unpackfebe_cb( void* udatabuf, 
                   int udatabuflen, 
                   void* udata ) 
{
   spindle_daemon_args *args = (spindle_daemon_args *) udata;
   
   char *buffer = (char *) udatabuf;
   int pos = 0;

   unpack_param(args->number, buffer, pos);
   unpack_param(args->port, buffer, pos);
   unpack_param(args->opts, buffer, pos);
   unpack_param(args->shared_secret, buffer, pos);
   unpack_param(args->location, buffer, pos);
   unpack_param(args->pythonprefix, buffer, pos);
   assert(pos == udatabuflen);

   return 0;    
}

int packfebe_cb(void *udata, 
                void *msgbuf, 
                int msgbufmax, 
                int *msgbuflen)
{  
   spindle_daemon_args *args = (spindle_daemon_args *) udata;

   *msgbuflen = sizeof(unsigned int) * 4;
   *msgbuflen += strlen(args->location) + 1;
   *msgbuflen += strlen(args->pythonprefix) + 1;
   assert(*msgbuflen < msgbufmax);
   
   char *buffer = (char *) msgbuf;
   int pos = 0;
   pack_param(args->number, buffer, pos);
   pack_param(args->port, buffer, pos);
   pack_param(args->opts, buffer, pos);
   pack_param(args->shared_secret, buffer, pos);
   pack_param(args->location, buffer, pos);
   pack_param(args->pythonprefix, buffer, pos);
   assert(pos == *msgbuflen);

   return 0;
}

