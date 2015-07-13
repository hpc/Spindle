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

#if !defined(LDCS_AUDIT_SERVER_REQUESTORS_H_)
#define LDCS_AUDIT_SERVER_REQUESTORS_H_

#include "ldcs_audit_server_md.h"
#include "ldcs_audit_server_process.h"

requestor_list_t new_requestor_list();
int been_requested(requestor_list_t list, char *file);
void add_requestor(requestor_list_t list, char *file, node_peer_t peer);
void clear_requestor(requestor_list_t list, char *file);
int get_requestors(requestor_list_t list, char *file, node_peer_t **requestor_list, int *requestor_list_size);
int peer_requested(requestor_list_t list, char *file, node_peer_t peer);

#endif
