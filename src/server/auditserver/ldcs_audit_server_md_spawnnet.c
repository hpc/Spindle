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

#include "ldcs_audit_server_md.h"

int ldcs_audit_server_md_init(unsigned int port, unsigned int num_ports, unique_id_t unique_id, ldcs_process_data_t *data)
{
  return -1;
}

int ldcs_audit_server_md_register_fd ( ldcs_process_data_t *data )
{
  return -1;
}

int ldcs_audit_server_md_unregister_fd ( ldcs_process_data_t *data )
{
  return -1;
}

int ldcs_audit_server_md_destroy ( ldcs_process_data_t *data )
{
  return -1;
}

int ldcs_audit_server_md_is_responsible ( ldcs_process_data_t *data, char *filename )
{
  return -1;
}

int ldcs_audit_server_md_trash_bytes(node_peer_t peer, size_t size)
{
  return -1;
}

int ldcs_audit_server_md_forward_query(ldcs_process_data_t *ldcs_process_data, ldcs_message_t* msg)
{
  return -1;
}

int ldcs_audit_server_md_complete_msg_read(node_peer_t peer, ldcs_message_t *msg, void *mem, size_t size)
{
  return -1;
}

int ldcs_audit_server_md_recv_from_parent(ldcs_message_t *msg)
{
  return -1;
}

int ldcs_audit_server_md_send(ldcs_process_data_t *ldcs_process_data, ldcs_message_t *msg, node_peer_t peer)
{
  return -1;
}

int ldcs_audit_server_md_broadcast(ldcs_process_data_t *ldcs_process_data, ldcs_message_t *msg)
{
  return -1;
}

int ldcs_audit_server_md_send_noncontig(ldcs_process_data_t *ldcs_process_data, ldcs_message_t *msg, 
                                        node_peer_t peer,
                                        void *secondary_data, size_t secondary_size)
{
  return -1;
}

int ldcs_audit_server_md_broadcast_noncontig(ldcs_process_data_t *ldcs_process_data, ldcs_message_t *msg,
                                             void *secondary_data, size_t secondary_size)
{
  return -1;
}

int ldcs_audit_server_md_get_num_children(ldcs_process_data_t *procdata)
{
  return -1;
}
