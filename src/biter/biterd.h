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
extern int biterd_init_comms(const char *tmpdir);

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
