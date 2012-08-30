LAUNCHMON=/usr/global/tools/launchmon/chaos_5_x86_64_ib/launchmon-1.0.0-20120821
export LMON_PREFIX=/collab/usr/global/tools/launchmon/chaos_5_x86_64_ib/launchmon-1.0.0-20120821
export LMON_LAUNCHMON_ENGINE_PATH=/collab/usr/global/tools/launchmon/chaos_5_x86_64_ib/launchmon-1.0.0-20120821/bin/launchmon

LD_LIBRARY_PATH=${LAUNCHMON}/lib:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH

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

LD_LIBRARY_PATH=../auditclient/lib:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH

LD_AUDIT=`pwd`/../auditclient/ldcs_audit_client_pipe.so
export LD_AUDIT

./spindle_fe ../auditclient/helloworld3_mpi 4 2 pdebug `pwd`/spindle_be  
