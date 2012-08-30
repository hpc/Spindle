#ifndef LDCS_AUDIT_SERVER_MD_MSOCKET_TOPO_H
#define LDCS_AUDIT_SERVER_MD_MSOCKET_TOPO_H

#include "ldcs_audit_server_md_msocket.h"

struct ldcs_msocket_bootstrap_struct
{
  int size;
  int allocsize;
  int* fromlist;
  int* tolist;
  char** tohostlist;
  int* toportlist;
};
typedef struct ldcs_msocket_bootstrap_struct ldcs_msocket_bootstrap_t;


int ldcs_audit_server_md_msocket_init_topo_bootstrap(ldcs_msocket_data_t *ldcs_msocket_data);
int ldcs_audit_server_md_msocket_run_topo_bootstrap(ldcs_msocket_data_t *ldcs_msocket_data, char *serbootinfo, int serbootlen);
int _ldcs_audit_server_md_msocket_run_topo_bootstrap(ldcs_msocket_data_t *ldcs_msocket_data, ldcs_msocket_bootstrap_t *bootstrap);

int compute_connections(int size, int **connlist, int *connlistsize, int *max_connections);
int compute_binom_tree(int size, int **connlist, int *connlistsize, int *max_connections);

int ldcs_audit_server_md_msocket_route_msg(ldcs_msocket_data_t *ldcs_msocket_data, ldcs_message_t *msg);
int ldcs_audit_server_md_msocket_route_msg_binom_tree(ldcs_msocket_data_t *ldcs_msocket_data, ldcs_message_t *msg);

#endif
