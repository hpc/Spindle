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

#ifndef LDCS_AUDIT_SERVER_STATELOOP_H
#define LDCS_AUDIT_SERVER_STATELOOP_H

typedef enum {
  LDCS_STATE_READY,
  LDCS_STATE_DONE,
  LDCS_STATE_CLIENT_INFO_MSG,
  LDCS_STATE_CLIENT_END_MSG,  
  LDCS_STATE_CLIENT_FILE_QUERY_MSG,
  LDCS_STATE_CLIENT_FILE_QUERY_EXACT_PATH_MSG,
  LDCS_STATE_CLIENT_FILE_QUERY_CHECK,
  LDCS_STATE_CLIENT_FORWARD_QUERY,
  LDCS_STATE_CLIENT_PROCESS_FILE,
  LDCS_STATE_CLIENT_PROCESS_DIR,
  LDCS_STATE_CLIENT_PROCESS_LOCAL_FILE_QUERY,
  LDCS_STATE_CLIENT_PROCESS_REJECTED_QUERY,
  LDCS_STATE_CLIENT_PROCESS_FULFILLED_QUERY,
  LDCS_STATE_CLIENT_PROCESS_NODIR_QUERY,
  LDCS_STATE_CLIENT_MYRANKINFO_MSG,
  LDCS_STATE_CLIENT_START_UPDATE,
  LDCS_STATE_CLIENT_UPDATE,
  LDCS_STATE_CLIENT_RECV_EXIT_BCAST,
  LDCS_STATE_UNKNOWN
} ldcs_state_t;

ldcs_state_t ldcs_server_process_state ( ldcs_process_data_t *ldcs_process_data, ldcs_message_t *msg, ldcs_state_t state );
char* _state_type_to_str (ldcs_state_t state);

#endif
