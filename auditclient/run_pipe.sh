LD_AUDIT=`pwd`/ldcs_audit_client_pipe.so
export LD_AUDIT

LD_LIBRARY_PATH=.:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH

LDCS_LOCATION=/tmp/myfifo
export LDCS_LOCATION
LDCS_NUMBER=7777
export LDCS_NUMBER

#LD_DEBUG=all
#export LD_DEBUG

SION_DEBUG=_debug_audit_client
export SION_DEBUG

 ./helloworld2
