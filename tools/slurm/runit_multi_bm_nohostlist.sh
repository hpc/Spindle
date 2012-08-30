#!/bin/bash
#set -x

#ID=S01
#BMPATH=/g/g92/frings1/LLNL/pynamic/benchmark4
#SYSTEM=cab
#DO_NON_LDCS=YES
#DO_LDCS=YES

#STRACE="strace -ff -T -ttt -E LD_LIBRARY_PATH=./ -o strace/pynamic.strace "
STRACE=""

# very specil at LLNL system: sometimes srun mps /tmp to /tmp/frings1
SRUNTMP=/tmp
RSHTMP=/tmp
DEBUGDIR=/p/lscratchb/frings1/log56_24t3
#DEBUGDIR=`pwd`/tmp/

if [ "*$BENCHNP" = "*" ]
then
    NODES=$SLURM_JOB_NUM_NODES
else
    NODES=$BENCHNP 
fi

if [ "*$BENCHNUMITER" = "*" ]
then
    NUMITER=1
else
    NUMITER=$BENCHNUMITER
fi

if [ "*$BENCHNUMSERVER" = "*" ]
then
    NUMSERVER=1
else
    NUMSERVER=$BENCHNUMSERVER
fi

if [ "*$BENCHTASKPERNODE" = "*" ]
then
    TASKPERNODE=$SLURM_CPUS_ON_NODE
else
    TASKPERNODE=$BENCHTASKPERNODE
fi

HELLOWORLDMPI=/g/g92/frings1/LLNL/LDCS/tools/other/helloworld_mpi

let TOTALTASKS=${NODES}*${TASKPERNODE}

SLEEPTIME=10

SPEC="${SYSTEM}_${ID}_n${NODES}"

REPORTFILE="logs/REPORT_${SPEC}.log"

echo "ID          = ${ID}"     > $REPORTFILE
echo "SYSTEM      = ${SYSTEM}" >> $REPORTFILE
echo "NODES       = ${NODES}"  >> $REPORTFILE
echo "TASKPERNODE = ${TASKPERNODE}" >> $REPORTFILE
echo "NUMITER     = ${NUMITER}" >> $REPORTFILE
echo "NUMSERVER   = ${NUMSERVER}" >> $REPORTFILE

cd $BMPATH
./generate_hostfiles.pl ${SPEC} ${NODES}  ${TASKPERNODE} 


echo "create ./tmp/cobo_param_${SPEC}.dat"
echo "LDCS_LOCATION=${RSHTMP}/myfifo" > ./tmp/cobo_param_${SPEC}.dat
echo "LDCS_NUMBER=7777" >> ./tmp/cobo_param_${SPEC}.dat 
echo "LDCS_NPORTS=10" >> ./tmp/cobo_param_${SPEC}.dat 
echo "LDCS_EXIT_AFTER_SESSION=1" >> ./tmp/cobo_param_${SPEC}.dat 

if [ "$DEBUG_SERVER" = "YES" ]
then
    echo "SION_DEBUG=${DEBUGDIR}/debug_audit_server_md" >> ./tmp/cobo_param_${SPEC}.dat 
    echo "COBO_CLIENT_DEBUG=7" >> ./tmp/cobo_param_${SPEC}.dat 
fi

CLEANUPLOGFILE="logs/cleanup_benchmark_${SPEC}.log"
CLEANUPERRLOGFILE="logs/cleanup_benchmark_${SPEC}.errlog"
echo "cleanup" > ${CLEANUPLOGFILE}
echo "cleanup" > ${CLEANUPERRLOGFILE}

LD_LIBRARY_PATH=./:$LD_LIBRARY_PATH 
export LD_LIBRARY_PATH

let NUMITERHELP=${NUMITER}-1
for LOOP in $(seq 0 ${NUMITERHELP})
do
    if [ "$DO_NON_LDCS" = "YES" ]
    then
	echo "Running Cleanup #${LOOP}"
	echo "Running Cleanup #${LOOP}" >> $REPORTFILE
	echo "Running Cleanup #${LOOP}" >> ${CLEANUPLOGFILE}
	echo "Running Cleanup #${LOOP}" >> ${CLEANUPERRLOGFILE}
	(
	    SLURM_HOSTFILE=/g/g92/frings1/LLNL/pynamic/benchmark4/tmp/hostlist_${SPEC}.dat srun --distribution arbitrary -n ${NODES} --drop-caches ${HELLOWORLDMPI}  
	) >> $CLEANUPLOGFILE 2>> $CLEANUPERRLOGFILE
	
	echo "Running without LDCS ${LOOP}"
	echo "" >> $REPORTFILE
	echo "Running without LDCS ${LOOP}" >> $REPORTFILE
	
	
	STARTFILE="tmp/client_benchmark_wo_ldcs_${SPEC}_${LOOP}.sh"
	TIMINGSFILE="logs/client_benchmark_wo_ldcs_${SPEC}_${LOOP}.timelog"
	LOGFILE="logs/client_benchmark_wo_ldcs_${SPEC}_${LOOP}.log"
	ERRLOGFILE="logs/client_benchmark_wo_ldcs_${SPEC}_${LOOP}.errlog"
	CMD="SLURM_HOSTFILE=/g/g92/frings1/LLNL/pynamic/benchmark4/tmp/slurm_hostlist_${SPEC}.dat srun --distribution arbitrary -v -n ${TOTALTASKS} ${STRACE} pynamic-pyMPI pynamic_driver.py \`date +%s.%N\`"
	echo $CMD > ${STARTFILE}
	chmod u+x ${STARTFILE}
	(
	    echo $CMD
	    echo "srun start time = `date +%s.%N`"
	    /usr/bin/time -o ${TIMINGSFILE} -v ${STARTFILE}
	    echo "srun end time = `date +%s.%N`"
	) > $LOGFILE 2> $ERRLOGFILE
	grep 'Elapsed' ${TIMINGSFILE}
	grep 'Elapsed' ${TIMINGSFILE} >> $REPORTFILE

	grep 'time =' ${LOGFILE}
	grep 'time =' ${LOGFILE} >> $REPORTFILE

    fi


    if [ "$DO_LDCS" = "YES" ]
    then

	echo "Running Cleanup #${LOOP}"
	echo "Running Cleanup #${LOOP}" >> $REPORTFILE
	echo "Running Cleanup #${LOOP}" >> ${CLEANUPLOGFILE}
	echo "Running Cleanup #${LOOP}" >> ${CLEANUPERRLOGFILE}
	(
	    SLURM_HOSTFILE=/g/g92/frings1/LLNL/pynamic/benchmark4/tmp/hostlist_${SPEC}.dat srun --distribution arbitrary -n ${NODES} --drop-caches ${HELLOWORLDMPI}  
	    pdsh rm -rf ${RSHTMP}/myfifo-tmp/ ${RSHTMP}/myfifo/
	) >> $CLEANUPLOGFILE 2>> $CLEANUPERRLOGFILE

	let SERVERSIZE=${NODES}/${NUMSERVER}
	
	split -a 1 -d -l ${SERVERSIZE} ./tmp/hostlist_${SPEC}.dat ./tmp/hostlist_${SPEC}.dat_

	PARAMFILE=/g/g92/frings1/LLNL/pynamic/benchmark4/tmp/cobo_param_${SPEC}.dat

	if [ "$DEBUG_CLIENT" = "YES" ]
	then
	    AUDITDEBUG="LDCS_AUDITDEBUG=${DEBUGDIR}/_debug_auditclient"
	elif [ "$DEBUG_CLIENT" = "ALL" ]
	then
	    AUDITDEBUG="SION_DEBUG=${DEBUGDIR}/_debug_auditclient"
	else
	    AUDITDEBUG=""
	fi

	if [ "*$BENCHPRELOAD" != "*" ]
	then
	    LDCS_PRELOADFILE="-preload $BENCHPRELOAD"
	    echo "setting LDCS_PRELOADFILE=$LDCS_PRELOADFILE"
	fi


	
	SERVER_LOG_FILES=""
	SERVER_ERR_FILES=""
	let NUMSERVERHELP=${NUMSERVER}-1
	for SUBLOOP in $(seq 0 ${NUMSERVERHELP})
	do
	    echo " Starting server in background #${LOOP} ${SUBLOOP}"
	    LOGFILE="logs/server_benchmark_${SPEC}_${LOOP}_${SUBLOOP}.log"
	    ERRLOGFILE="logs/server_benchmark_${SPEC}_${LOOP}_${SUBLOOP}.errlog"
	    MYH=`head -1 ./tmp/hostlist_${SPEC}.dat_${SUBLOOP}`
	    CMD="/g/g92/frings1/LLNL/LDCS/tools/cobo/test/server_rsh_ldcs_preload ${LDCS_PRELOADFILE} -np ${SERVERSIZE} -paramfile ${PARAMFILE} -hostfile /g/g92/frings1/LLNL/pynamic/benchmark4/tmp/hostlist_${SPEC}.dat_${SUBLOOP} /g/g92/frings1/LLNL/LDCS/auditserver/ldcs_audit_server_par_pipe_md"
#	    SION_DEBUG=./tmp/debug_fe_${SPEC}_${LOOP}_${SUBLOOP}.log
#	    export SION_DEBUG

	    echo "cd $BMPATH; rsh ${MYH} ${CMD}"
	    echo "cd $BMPATH; rsh ${MYH} ${CMD}"  > $LOGFILE
	    echo "cd $BMPATH; rsh ${MYH} ${CMD}"  > $ERRLOGFILE
	    rsh ${MYH} "(ulimit -u 2048 -n 4096;ulimit -a; ${CMD})" >> $LOGFILE 2>> $ERRLOGFILE &
	    SERVERPID=$!
	    echo "SERVERPID=${SERVERPID}"
	    SERVER_LOG_FILES="${SERVER_LOG_FILES} $LOGFILE"
	    SERVER_ERR_FILES="${SERVER_ERR_FILES} $ERRLOGFILE"
	done

	NUMSLEEPITER=0
	echo "grep SERVER_STARTUP_READY ${SERVER_ERR_FILES} | wc -l"
	NUMSERVER_READY=`grep SERVER_STARTUP_READY ${SERVER_ERR_FILES} | wc -l`
	grep SERVER_STARTUP_READY ${SERVER_ERR_FILES}
	while [ $NUMSERVER_READY -lt $NUMSERVER ]
	do 
	    echo "Time ${NUMSLEEPITER}s: Not all server ready ($NUMSERVER_READY of $NUMSERVER) --> sleep(1)"
	    sleep 1 

	    let NUMSLEEPITER=${NUMSLEEPITER}+1
	    NUMSERVER_READY=`grep SERVER_STARTUP_READY ${SERVER_ERR_FILES}  | wc -l`
	    grep SERVER_STARTUP_READY ${SERVER_ERR_FILES}
	done
	echo "sleep(1)"
	sleep 1
	echo "Server ready ($NUMSERVER_READY of $NUMSERVER) after $NUMSLEEPITER seconds"

	echo "Running with LDCS ${LOOP}"
	echo "" >> $REPORTFILE
	echo "Running with LDCS ${LOOP}" >> $REPORTFILE
	
	LDCS_NUMBER=7777 
	
	STARTFILE="tmp/client_benchmark_with_ldcs_${SPEC}_${LOOP}.sh"
	TIMINGSFILE="logs/client_benchmark_with_ldcs_${SPEC}_${LOOP}.timelog"
	LOGFILE="logs/client_benchmark_with_ldcs_${SPEC}_${LOOP}.log"
	ERRLOGFILE="logs/client_benchmark_with_ldcs_${SPEC}_${LOOP}.errlog"

#	CMD="LDCS_LOCATION=${SRUNTMP}/myfifo LDCS_NUMBER=7777 LD_AUDIT=/g/g92/frings1/LLNL/LDCS/auditclient/ldcs_audit_client_pipe.so $AUDITDEBUG SLURM_HOSTFILE=/g/g92/frings1/LLNL/pynamic/benchmark4/tmp/slurm_hostlist_${SPEC}.dat srun --distribution arbitrary -v -n ${TOTALTASKS} ${STRACE} pynamic-pyMPI pynamic_driver.py \`date +%s.%N\`"

	CMD="LDCS_LOCATION=${SRUNTMP}/myfifo LDCS_NUMBER=7777 LD_AUDIT=/g/g92/frings1/LLNL/LDCS/auditclient/ldcs_audit_client_pipe.so $AUDITDEBUG srun --distribution block -v -N ${NODES} -x sierra1760,sierra1761,sierra1762,sierra1763 -n ${TOTALTASKS} ${STRACE} pynamic-pyMPI pynamic_driver.py \`date +%s.%N\`"
	echo $CMD > ${STARTFILE}
	chmod u+x ${STARTFILE}

	(
	    echo $CMD
	    echo "srun start time = `date +%s.%N`"
	    /usr/bin/time -o ${TIMINGSFILE} -v ${STARTFILE}
	    echo "srun end time = `date +%s.%N`"
	) >> $LOGFILE 2> $ERRLOGFILE

	grep 'Elapsed' ${TIMINGSFILE}
	grep 'Elapsed' ${TIMINGSFILE} >> $REPORTFILE

	grep 'time =' ${LOGFILE}
	grep 'time =' ${LOGFILE} >> $REPORTFILE

	echo " Wait for server in background #${SERVERPID}"
	echo " Wait for server in background #${SERVERPID}" >> $REPORTFILE
	FAIL=0
	for job in `jobs -p`
	do
	    echo $job
	    wait $job || let "FAIL+=1"
	done

	echo $FAIL
	
	if [ "$FAIL" == "0" ];
	then
	    echo "YAY!"
	else
	    echo "FAIL! ($FAIL)"
	fi

	grep 'LDCS_FE_' ${SERVER_LOG_FILES}
	grep 'LDCS_FE_' ${SERVER_LOG_FILES} >> $REPORTFILE

    fi
done