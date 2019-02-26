#if !defined(MSGBUNDLE_H_)
#define MSGBUNDLE_H_

#include "ldcs_audit_server_process.h"
#include "ldcs_audit_server_md.h"
#include "ldcs_api.h"

void msgbundle_init(ldcs_process_data_t *procdata);
void msgbundle_done(ldcs_process_data_t *procdata);
void msgbundle_force_flush(ldcs_process_data_t *procdata);
   
int spindle_send_noncontig(ldcs_process_data_t *procdata, ldcs_message_t *msg, node_peer_t node,
                           void *secondary_data, size_t secondary_size);
int spindle_broadcast_noncontig(ldcs_process_data_t *procdata, ldcs_message_t *msg,
                                void *secondary_data, size_t secondary_size);

int spindle_send(ldcs_process_data_t *procdata, ldcs_message_t *msg, node_peer_t node);
int spindle_broadcast(ldcs_process_data_t *procdata, ldcs_message_t *msg);

int spindle_forward_query(ldcs_process_data_t *procdata, ldcs_message_t *msg);

#endif
