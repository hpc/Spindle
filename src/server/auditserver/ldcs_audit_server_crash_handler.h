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

#ifndef LDCS_AUDIT_SERVER_CRASH_HANDLER_H
#define LDCS_AUDIT_SERVER_CRASH_HANDLER_H

#include "ldcs_api.h"
#include "ldcs_audit_server_process.h"
#include "ldcs_audit_server_md.h"

/* CRASH_REPORT from a local client */
int handle_client_crash_report(ldcs_process_data_t *procdata,
                               int nc, ldcs_message_t *msg);

/* CRASH_REPORT from a child */
int handle_crash_report_recv(ldcs_process_data_t *procdata,
                             node_peer_t peer, ldcs_message_t *msg);

/* CRASH_RESPONSE from a parent */
int handle_crash_response_recv(ldcs_process_data_t *procdata,
                               node_peer_t peer, ldcs_message_t *msg);

/* Free the crash-site table at shutdown */
void crash_free_tables(ldcs_process_data_t *procdata);

#endif
