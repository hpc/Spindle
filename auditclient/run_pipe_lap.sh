LD_AUDIT=`pwd`/ldcs_audit_client_pipe.so
export LD_AUDIT

LD_LIBRARY_PATH=./lib:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH

LDCS_LOCATION=/tmp/myfifo
export LDCS_LOCATION

LDCS_LOCATION_MOD=2
export LDCS_LOCATION_MOD

LDCS_NUMBER=7777
export LDCS_NUMBER

#LD_DEBUG=libs
#export LD_DEBUG

SION_DEBUG=_debug_audit_client_mpi
export SION_DEBUG

mpirun -np 4  ./helloworld3_mpi

