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

#ifndef SPINDLE_USRDATA_H
#define SPINDLE_USRDATA_H

#ifdef __cplusplus
extern "C" {
#endif

#define HOSTNAME_LEN 100

struct ldcs_host_port_list_struct
{
  int size;
  char* hostlist;
  int*  portlist;
};
typedef struct ldcs_host_port_list_struct ldcs_host_port_list_t;

int packbefe_cb ( void* udata, 
		  void* msgbuf, 
		  int msgbufmax, 
		  int* msgbuflen );

int unpackfebe_cb  ( void* udatabuf, 
		     int udatabuflen, 
		     void* udata );

int packfebe_cb ( void *udata, 
		  void *msgbuf, 
		  int msgbufmax, 
		  int *msgbuflen );

int unpackbefe_cb  ( void* udatabuf, 
		     int udatabuflen, 
		     void* udata );

#ifdef __cplusplus
}
#endif

#endif
