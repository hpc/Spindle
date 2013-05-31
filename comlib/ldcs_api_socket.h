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

#ifndef LDCS_API_SOCKET_H
#define LDCS_API_SOCKET_H

typedef enum {
   LDCS_SOCKET_FD_TYPE_SERVER,
   LDCS_SOCKET_FD_TYPE_CONN,
   LDCS_SOCKET_FD_TYPE_UNKNOWN
} fd_list_entry_type_t;

struct fdlist_entry_t
{
  int   inuse;
  fd_list_entry_type_t type;

  /* server part */
  int   server_fd; 
  int   conn_list_size; 
  int   conn_list_used; 
  int  *conn_list; 

  /* connection part */
  int   fd;
  int   serverid;
};

#endif

