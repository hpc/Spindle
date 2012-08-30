#ifndef LDCS_AUDIT_SERVER_FILEMNGT_H
#define LDCS_AUDIT_SERVER_FILEMNGT_H


int ldcs_audit_server_filemngt_init (char* location);

int ldcs_audit_server_filemngt_read_file ( char *filename, char *dirname, char *fullname, 
					   int domangle, ldcs_message_t* msg );
int ldcs_audit_server_filemngt_store_file ( ldcs_message_t* msg, char **filename, 
					    char **dirname, char **localpath, int *domangle );

#endif
