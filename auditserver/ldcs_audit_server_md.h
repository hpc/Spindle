#ifndef LDCS_AUDIT_SERVER_MD_H
#define LDCS_AUDIT_SERVER_MD_H

#include "ldcs_api.h"
#include "ldcs_audit_server_process.h"


int ldcs_audit_server_md_init ( ldcs_process_data_t *data );
int ldcs_audit_server_md_register_fd ( ldcs_process_data_t *data );
int ldcs_audit_server_md_unregister_fd ( ldcs_process_data_t *data );
int ldcs_audit_server_md_destroy ( ldcs_process_data_t *data );

int ldcs_audit_server_md_distribute ( ldcs_process_data_t *ldcs_process_data, ldcs_message_t* msg);
int ldcs_audit_server_md_is_responsible ( ldcs_process_data_t *data, char *filename );
int ldcs_audit_server_md_distribution_required ( ldcs_process_data_t *data, char *filename );
int ldcs_audit_server_md_forward_query(ldcs_process_data_t *ldcs_process_data, ldcs_message_t* msg);

/* Front end connection */
int ldcs_audit_server_fe_md_open ( char **hostlist, int hostlistsize, void **data  );

int ldcs_audit_server_fe_md_preload ( char *filename, void *data  );

int ldcs_audit_server_fe_md_close ( void *data  );

int ldcs_register_external_fabric_CB( int cb ( char*, int, int*, int*, char***, int**, void *), 
					      void *data);

#endif
