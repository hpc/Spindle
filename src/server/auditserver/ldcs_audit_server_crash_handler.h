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

/* CRASH_LOG table from a child */
int handle_crash_log_recv(ldcs_process_data_t *procdata,
                          node_peer_t peer, ldcs_message_t *msg);

/* Crash-site table shared by the dedup handler and the crash log */
crash_site_entry_t *crash_site_find(ldcs_process_data_t *procdata,
                                    const char *site, size_t site_len);
crash_site_entry_t *crash_site_insert(ldcs_process_data_t *procdata,
                                      const char *site, size_t site_len);

/* Add a display rank to a site's crash-log rank list */
void crash_log_append_rank(crash_site_entry_t *e, int32_t rank);

/* Send accumulated crash-log ranks to the parent. */
void crash_log_flush_to_parent(ldcs_process_data_t *procdata);

/* Forward or write pending crash-log data */
void crash_log_updated(ldcs_process_data_t *procdata);

/* Write crash log file */
void crash_log_root_write(ldcs_process_data_t *procdata);

/* Free the crash-site table at shutdown */
void crash_free_tables(ldcs_process_data_t *procdata);

#endif
