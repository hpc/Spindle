LD_AUDIT=`pwd`/ldcs_audit_client_pipe.so
export LD_AUDIT

LD_LIBRARY_PATH=.:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH

LDCS_LOCATION=/tmp/myfifo
export LDCS_LOCATION
LDCS_NUMBER=7777
export LDCS_NUMBER

LD_DEBUG=libs
export LD_DEBUG
/usr/bin/srun -n 4  /g/g92/frings1/LLNL/LDCS/auditclient/helloworld3_mpi
