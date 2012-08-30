#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/inotify.h>
#include <errno.h>

#include "ldcs_api.h"
#include "ldcs_api_listen.h"
#include "ldcs_audit_server_process.h"
#include "ldcs_audit_server_md.h"

int ldcs_audit_server_md_init ( ldcs_process_data_t *data ) {
  int rc=0;

  return(rc);
}

int ldcs_audit_server_md_register_fd ( ldcs_process_data_t *data ) {
  int rc=0;

  return(rc);
}

int ldcs_audit_server_md_unregister_fd ( ldcs_process_data_t *data ) {
  int rc=0;

  return(rc);
}

int ldcs_audit_server_md_destroy ( ldcs_process_data_t *data ) {
  int rc=0;

  return(rc);
}

int ldcs_audit_server_md_distribute ( ldcs_process_data_t *ldcs_process_data, ldcs_message_t* msg) {
  int rc=0;

  return(rc);
}

int ldcs_audit_server_md_is_responsible ( ldcs_process_data_t *data, char *msg ) {
  int rc=0;

  return(rc);
}

int ldcs_audit_server_md_distribution_required ( ldcs_process_data_t *data, char *msg ) {
  int rc=0;

  return(rc);
}

int ldcs_audit_server_md_forward_query(ldcs_process_data_t *ldcs_process_data, ldcs_message_t* msg) {
  int rc=0;

  return(rc);
}


int ldcs_audit_server_fe_md_open ( char **hostlist, int numhosts, void **data  ) {
  int rc=0;

  return(rc);
}

int ldcs_audit_server_fe_md_preload ( char *filename, void *data  ) {
  int rc=0;

  return(rc);
}

int ldcs_audit_server_fe_md_close ( void *data  ) {
  int rc=0;

  return(rc);
}



