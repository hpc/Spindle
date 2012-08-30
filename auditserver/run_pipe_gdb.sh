LDCS_LOCATION=/tmp/myfifo
export LDCS_LOCATION
LDCS_NUMBER=7777
export LDCS_NUMBER

LDCS_NCLIENTS=1
export LDCS_NCLIENTS


totalview  ./ldcs_audit_server_par_pipe
#ddd  ./ldcs_audit_server_pipe
#gdb  ./ldcs_audit_server_pipe
