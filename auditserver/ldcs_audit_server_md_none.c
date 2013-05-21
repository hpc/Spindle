/*
This file is part of Spindle.  For copyright information see the COPYRIGHT 
file in the top level directory, or at 
<TODO:URL>.

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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/inotify.h>
#include <errno.h>

#include "ldcs_api.h"
#include "ldcs_api_listen.h"
#include "ldcs_audit_server_process.h"
#include "ldcs_audit_server_md.h"

int ldcs_audit_server_md_init ( ldcs_process_data_t *data ) {
  int rc=0;

  return(rc);
}

int ldcs_audit_server_md_register_fd ( ldcs_process_data_t *data ) {
  int rc=0;

  return(rc);
}

int ldcs_audit_server_md_unregister_fd ( ldcs_process_data_t *data ) {
  int rc=0;

  return(rc);
}

int ldcs_audit_server_md_destroy ( ldcs_process_data_t *data ) {
  int rc=0;

  return(rc);
}

int ldcs_audit_server_md_is_responsible ( ldcs_process_data_t *data, char *msg ) {
  int rc=0;

  return(rc);
}

int ldcs_audit_server_md_forward_query(ldcs_process_data_t *ldcs_process_data, ldcs_message_t* msg) {
  int rc=0;

  return(rc);
}


int ldcs_audit_server_fe_md_open ( char **hostlist, int numhosts, unsigned int port, void **data  ) {
  int rc=0;

  return(rc);
}

int ldcs_audit_server_fe_md_preload ( char *filename, void *data  ) {
  int rc=0;

  return(rc);
}

int ldcs_audit_server_fe_md_close ( void *data  ) {
  int rc=0;

  return(rc);
}



