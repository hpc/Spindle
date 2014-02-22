#if !defined(BITERD_H_)
#define BITERD_H_

#if defined(__cplusplus)
extern "C" {
#endif

#include <sys/select.h>

extern int biterd_newsession(const char *tmpdir, int session_id);
extern int biterd_num_clients(int session_id);

extern int biterd_fill_in_read_set(int session_id, fd_set *readset, int *max_fd);
extern int biterd_has_data_avail(int session_id, fd_set *readset);
extern int biterd_get_fd(int session_id);
extern int biterd_get_aux_fd();
extern int biterd_find_client_w_data(int session_id);
extern int biterd_get_session_proc_w_aux_data(int *session, int *proc);

extern int biterd_get_rank(int compute_node_id, int client_id);

extern int biterd_write(int session_id, int client_id, const void *buf, size_t count);
extern int biterd_read(int session_id, int client_id, void *buf, size_t count);

extern int biterd_clean(int session_id);
extern const char *biterd_lasterror_str();

extern int biterd_num_compute_nodes();

#if defined(__cplusplus)
}
#endif

#endif
