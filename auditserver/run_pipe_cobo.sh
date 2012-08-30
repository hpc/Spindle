LDCS_LOCATION=/tmp/myfifo
export LDCS_LOCATION
LDCS_NUMBER=7777
export LDCS_NUMBER

LDCS_NCLIENTS=1
export LDCS_NCLIENTS

../tools/cobo/test/server_rsh_ldcs -np 2 -paramfile ./cobo_param_laptop.dat -hostfile ./hostlist_laptop.dat ./ldcs_audit_server_par_pipe_md



