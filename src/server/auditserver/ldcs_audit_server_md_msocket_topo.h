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

#ifndef LDCS_AUDIT_SERVER_MD_MSOCKET_TOPO_H
#define LDCS_AUDIT_SERVER_MD_MSOCKET_TOPO_H

#include "ldcs_audit_server_md_msocket.h"

struct ldcs_msocket_bootstrap_struct
{
  int size;
  int allocsize;
  int* fromlist;
  int* tolist;
  char** tohostlist;
  int* toportlist;
};
typedef struct ldcs_msocket_bootstrap_struct ldcs_msocket_bootstrap_t;


int ldcs_audit_server_md_msocket_init_topo_bootstrap(ldcs_msocket_data_t *ldcs_msocket_data);
int ldcs_audit_server_md_msocket_run_topo_bootstrap(ldcs_msocket_data_t *ldcs_msocket_data, char *serbootinfo, int serbootlen);
int _ldcs_audit_server_md_msocket_run_topo_bootstrap(ldcs_msocket_data_t *ldcs_msocket_data, ldcs_msocket_bootstrap_t *bootstrap);

int compute_connections(int size, int **connlist, int *connlistsize, int *max_connections);
int compute_binom_tree(int size, int **connlist, int *connlistsize, int *max_connections);

int ldcs_audit_server_md_msocket_route_msg(ldcs_msocket_data_t *ldcs_msocket_data, ldcs_message_t *msg);
int ldcs_audit_server_md_msocket_route_msg_binom_tree(ldcs_msocket_data_t *ldcs_msocket_data, ldcs_message_t *msg);

#endif
