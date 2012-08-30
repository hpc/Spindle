#ifndef LDCS_AUDIT_SERVER_MD_MSOCKET_UTIL_H
#define LDCS_AUDIT_SERVER_MD_MSOCKET_UTIL_H

#include "ldcs_audit_server_md_msocket.h"
#include "ldcs_audit_server_md_msocket_topo.h"

int ldcs_audit_server_md_msocket_create_server(char * location, int *portlist, int num_ports, int *portused); 

int ldcs_audit_server_md_msocket_serialize_hostlist(char **hostlist, int numhosts, char **rdata, int *rsize);
char* ldcs_audit_server_md_msocket_expand_hostname(char *rdata, int rsize, int rank);


ldcs_msocket_bootstrap_t *ldcs_audit_server_md_msocket_new_bootstrap(int max_connections);
int ldcs_audit_server_md_msocket_free_bootstrap(ldcs_msocket_bootstrap_t *bootstrap);
int ldcs_audit_server_md_msocket_dump_bootstrap(ldcs_msocket_bootstrap_t *bootstrap);
int ldcs_audit_server_md_msocket_serialize_bootstrap(ldcs_msocket_bootstrap_t *bootstrap, char **rdata, int *rsize);
int ldcs_audit_server_md_msocket_deserialize_bootstrap(char *data, int datasize, ldcs_msocket_bootstrap_t **rbootstrap);

int ldcs_audit_server_md_msocket_get_free_connection_table_entry (ldcs_msocket_data_t *ldcs_msocket_data);

int ldcs_audit_server_md_msocket_connect(char *hostname, int *portlist, int num_ports);


#endif
