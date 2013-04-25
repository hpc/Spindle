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

/**
 * This functions are the API for Spindle's network communication.  They assume
 * all communication will flow in some tree-like form.  Requests for information
 * flow up the tree towards the root, while information flows down the tree towards
 * the roots.   This information is usually requests and contents for  files and 
 * directory listings.
 *
 * There could be multiple implementations of this layer, depending on how Spindle
 * is configured.  As of the writing of this comment, there is a COBO based 
 * implementation in ldcs_audit_server_md_cobo.c
 *
 * This layer receives messages via a registered callback.  The 
 * ldcs_audit_server_md_register_fd associates a callback function with a file
 * descriptor.  This callback will be triggered whenever the fd has data
 * available.  That callback should read the data off the network, put it into
 * a ldcs_message_t* and then call handle_server_message on the packet.
 *
 * Messages are sent from this interface through a set of functions in the 
 * below API, which are called by the handlers.
 * 
 *  * = The zero-copy mechanism requires special handling when reading a 
 *      LDCS_MSG_FILE_DATA type.  In those cases just read the header. 
 *      Do not read the file contents off the network.  Leave it there, and 
 *      ldcs_audit_server_md_complete_msg_read will later be used to read 
 *      the packet payload.
 **/

#ifndef LDCS_AUDIT_SERVER_MD_H
#define LDCS_AUDIT_SERVER_MD_H

#include "ldcs_api.h"
#include "ldcs_audit_server_process.h"

typedef void* node_peer_t;
#define NODE_PEER_CLIENT ((node_peer_t) 1)
#define NODE_PEER_NULL NULL

/* Any initialization can be done here. */
int ldcs_audit_server_md_init ( ldcs_process_data_t *data );

/* register_fd should, for every fd we want Spindle to recv messages on, call
   ldcs_listen_register_fd with the fd and a callback function to be triggered
   when a message arrives. */
int ldcs_audit_server_md_register_fd ( ldcs_process_data_t *data );

/* Called during network shutdown.  Should remove any fd's via the
   ldcs_listen_unregister_fd call */
int ldcs_audit_server_md_unregister_fd ( ldcs_process_data_t *data );

/* Any shutdown code can be done here */
int ldcs_audit_server_md_destroy ( ldcs_process_data_t *data );

/* Returns true if the current server should take responsibility for a file or
   directory and read it off disk.  Returns false if another server should 
   read the file */
int ldcs_audit_server_md_is_responsible ( ldcs_process_data_t *data, char *filename );

int ldcs_audit_server_md_trash_bytes(node_peer_t peer, size_t size);

/* Send a message to the parent server */
int ldcs_audit_server_md_forward_query(ldcs_process_data_t *ldcs_process_data, ldcs_message_t* msg);

/* This function should read some remainder of a partially read message.  Used by the zero-copy
   file mechanism. */
int ldcs_audit_server_md_complete_msg_read(node_peer_t peer, ldcs_message_t *msg, void *mem, size_t size);

/* Used to send messages down to child servers.  Send to either all children, or send to a select child.*/
int ldcs_audit_server_md_send(ldcs_process_data_t *ldcs_process_data, ldcs_message_t *msg, node_peer_t peer);
int ldcs_audit_server_md_broadcast(ldcs_process_data_t *ldcs_process_data, ldcs_message_t *msg);

/* Used for non-continugious-memory sends and recvs.  This is useful for implementing zero-copy
   for messages containing file contents. */
int ldcs_audit_server_md_send_noncontig(ldcs_process_data_t *ldcs_process_data, ldcs_message_t *msg, 
                                        node_peer_t peer,
                                        void *secondary_data, size_t secondary_size);
int ldcs_audit_server_md_broadcast_noncontig(ldcs_process_data_t *ldcs_process_data, ldcs_message_t *msg,
                                             void *secondary_data, size_t secondary_size);

/* Called in the spindle FE during network initialization.  The opaque 'data' returned
   by _fe_md_open will also be passed to _fe_md_preload and _fe_md_close */
int ldcs_audit_server_fe_md_open ( char **hostlist, int hostlistsize, void **data  );

int ldcs_audit_server_fe_md_preload ( char *filename, void *data  );

/* Called from the spindle FE during shutdown */
int ldcs_audit_server_fe_md_close ( void *data  );

#endif
