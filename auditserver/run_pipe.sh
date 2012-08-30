LDCS_LOCATION=/tmp/myfifo
export LDCS_LOCATION
LDCS_NUMBER=7777
export LDCS_NUMBER

LDCS_NCLIENTS=1
export LDCS_NCLIENTS

SION_DEBUG=_debug_audit_server
export SION_DEBUG

mkdir /tmp/myfifo

./ldcs_audit_server_par_pipe