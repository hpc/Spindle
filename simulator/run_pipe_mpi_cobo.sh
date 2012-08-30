LD_LIBRARY_PATH=/g/g92/frings1/LLNL/pynamic/benchmark3:$LD_LIBRARY_PATH
LD_LIBRARY_PATH=./lib:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH

LDCS_LOCATION=/tmp/myfifo
export LDCS_LOCATION

LDCS_NUMBER=7777
export LDCS_NUMBER

#LD_DEBUG=libs
#export LD_DEBUG

#SION_DEBUG=_debug_audit_client_mpi
#export SION_DEBUG

cp searchlist_cab_short.dat searchlist.dat
#cp searchlist_py.dat searchlist.dat

/usr/bin/srun -N 2 -n 11 --distribution block ./simulator_pipe_mpi_cobo

#/usr/bin/srun -N 2 -n 8 --distribution block  memcheck_all ./simulator_pipe_mpi

#totalview /usr/bin/srun -a -N 2 -n 8 --distribution block  ./simulator_pipe_mpi

