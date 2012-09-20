LAUNCHMON=/home/zam/zdv087/releases/launchmon
SPINDLE=/home/zam/zdv087/releases/spindle

export RM_TYPE=openrte
export MPI_JOB_LAUNCHER_PATH=mpiexec
export LMON_PREFIX=/home/zam/zdv087/releases/launchmon
export LMON_LAUNCHMON_ENGINE_PATH=/home/zam/zdv087/releases/launchmon/bin/launchmon

LD_LIBRARY_PATH=${LAUNCHMON}/lib:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH

export LMON_FE_HOSTNAME_TO_CONN=localhost

LDCS_LOCATION=/tmp/myfifo
export LDCS_LOCATION

LDCS_NUMBER=7777
export LDCS_NUMBER

#LD_DEBUG=libs
#export LD_DEBUG

SION_DEBUG=_debug_audit_client_mpi
export SION_DEBUG

#LMON_DONT_STOP_APP=1
#export LMON_DONT_STOP_APP

LMON_VERBOSE=3
export LMON_VERBOSE


LD_LIBRARY_PATH=$SPINDLE/lib:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH

LD_AUDIT=$SPINDLE/lib/libldcs_audit_client_pipe.so
export LD_AUDIT

./spindle_fe ../auditclient/helloworld3_mpi 8 1 pdebug `pwd`/spindle_be  
