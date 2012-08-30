LD_LIBRARY_PATH=./lib:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH

LDCS_LOCATION=/var/run/ldcs/myfifo
export LDCS_LOCATION

LDCS_LOCATION_MOD=8
export LDCS_LOCATION_MOD

LDCS_NUMBER=7777
export LDCS_NUMBER

#LD_DEBUG=libs
#export LD_DEBUG

SION_DEBUG=_debug_audit_client_mpi
export SION_DEBUG

cp searchlist_lap.dat searchlist.dat

mpirun -np 5  ./simulator_pipe_mpi_cobo
