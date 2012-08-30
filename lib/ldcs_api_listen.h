#ifndef LDCS_API_LISTEN_H
#define LDCS_API_LISTEN_H

int ldcs_listen_register_fd( int fd, 
			     int id, 
			     int _ldcs_server_CB ( int fd, int id, void *data ), 
			     void * data);

int ldcs_listen_register_exit_loop_cb( int cb_func ( int num_fds, void *data ), 
				       void * data);

int ldcs_listen_unregister_fd( int fd );

int ldcs_listen_signal_end_listen_loop( );

int ldcs_listen();

#endif
