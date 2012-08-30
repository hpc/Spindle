LD_AUDIT=`pwd`/ldcs_audit_client_socket.so

LD_LIBRARY_PATH=`pwd`:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH

LDCS_LOCATION=localhost
export LDCS_LOCATION
LDCS_NUMBER=7777
export LDCS_NUMBER

export LD_AUDIT
./helloworld2
