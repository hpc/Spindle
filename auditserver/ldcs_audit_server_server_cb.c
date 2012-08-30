#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/inotify.h>
#include <errno.h>

#include "ldcs_api.h"
#include "ldcs_api_listen.h"
#include "ldcs_audit_server_process.h"

int _ldcs_server_CB ( int infd, int serverid, void *data ) {
  int rc=0;
  ldcs_process_data_t *ldcs_process_data = (ldcs_process_data_t *) data ;
  int nc, fd, more_avail;
  double cb_starttime;

  cb_starttime=ldcs_get_time();

  more_avail=1;
  while(more_avail) {
    
    /* add new client */
    if (ldcs_process_data->client_table_used >= ldcs_process_data->client_table_size) {
      ldcs_process_data->client_table = realloc(ldcs_process_data->client_table, 
						(ldcs_process_data->client_table_used + 16) * sizeof(ldcs_client_t)
						);
      ldcs_process_data->client_table_size = ldcs_process_data->client_table_used + 16;
      for(nc=ldcs_process_data->client_table_used;(nc<ldcs_process_data->client_table_used + 16);nc++) {
	ldcs_process_data->client_table[nc].state=LDCS_CLIENT_STATUS_FREE;
      }
    }
    for(nc=0;(nc<ldcs_process_data->client_table_size);nc++) {
    if (ldcs_process_data->client_table[nc].state==LDCS_CLIENT_STATUS_FREE) break;
    }
    if(nc==ldcs_process_data->client_table_size) _error("internal error with client table (table full)");
    
    ldcs_process_data->client_table[nc].connid       = ldcs_open_server_connections(serverid,&more_avail);
    ldcs_process_data->client_table[nc].state        = LDCS_CLIENT_STATUS_ACTIVE;
    ldcs_process_data->client_table[nc].null_msg_cnt = 0;    
    ldcs_process_data->client_table[nc].query_open   = 0;
    ldcs_process_data->client_table[nc].lrank        = ldcs_process_data->client_counter;
    ldcs_process_data->client_table_used++;
    ldcs_process_data->client_counter++;
    printf("SERVER[%02d]: open  client connection on host %s #c=%02d at %12.4f\n", ldcs_process_data->md_rank, 
	   ldcs_process_data->hostname, 
	   ldcs_process_data->client_counter,ldcs_get_time());
    
    /* register client fd to listener */
    fd=ldcs_get_fd(ldcs_process_data->client_table[nc].connid);
    ldcs_listen_register_fd(fd, nc, &_ldcs_client_CB, (void *) ldcs_process_data);
    ldcs_process_data->server_stat.num_connections++;
  }

  /* remember timestamp of connect of first client */
  if(ldcs_process_data->server_stat.starttime<0) {
    ldcs_process_data->server_stat.starttime=cb_starttime;
  }
  ldcs_process_data->server_stat.server_cb.cnt++;
  ldcs_process_data->server_stat.server_cb.time+=(ldcs_get_time()-cb_starttime);

  return(rc);
}



