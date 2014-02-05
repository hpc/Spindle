#if !defined(DAEMON_IDS_H_)
#define DAEMON_IDS_H_

#include <stdint.h>

extern int biterd_num_compute_nodes();
extern int biterd_ranks_in_cn(int cn_id);
extern int biterd_unique_num_for_cn(int cn_id);
extern int biterd_get_rank(int compute_node_id, int client_id);
extern int biterd_register_rank(int session_id, uint32_t client_id, uint32_t rank);

#endif
