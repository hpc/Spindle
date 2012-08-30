LD_LIBRARY_PATH=./lib:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH

LDCS_LOCATION=/tmp/myfifo
export LDCS_LOCATION

LDCS_NUMBER=7777
export LDCS_NUMBER

#LD_DEBUG=libs
#export LD_DEBUG

SION_DEBUG=_debug_audit_client_mpi
export SION_DEBUG
totalview /usr/bin/srun -a -N 2 -n 4  ./ldcs_simple_client_pipe_mpi
#srun -N 2 -n 4  ./ldcs_simple_client_pipe_mpi

