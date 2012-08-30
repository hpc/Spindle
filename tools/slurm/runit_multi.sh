#!/bin/bash
#set -x

#ID=S01
#BMPATH=/g/g92/frings1/LLNL/pynamic/benchmark3
#SYSTEM=cab
#DO_NON_LDCS=YES
#DO_LDCS=YES

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

TASKPERNODE=$SLURM_CPUS_ON_NODE
let TOTALTASKS=${NODES}*${TASKPERNODE}

SLEEPTIME=30

SPEC="${SYSTEM}_${ID}_n${NODES}"

REPORTFILE="logs/report_${SPEC}.log"

echo "ID          = ${ID}"     > $REPORTFILE
echo "SYSTEM      = ${SYSTEM}" >> $REPORTFILE
echo "NODES       = ${NODES}"  >> $REPORTFILE
echo "TASKPERNODE = ${TASKPERNODE}" >> $REPORTFILE
echo "NUMITER     = ${NUMITER}" >> $REPORTFILE
echo "NUMSERVER   = ${NUMSERVER}" >> $REPORTFILE

cd $BMPATH
./generate_files.pl ${SPEC}

CLEANUPLOGFILE="logs/client_benchmark_cleanup_${SPEC}.log"
CLEANUPERRLOGFILE="logs/client_benchmark_cleanup_${SPEC}.errlog"
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
	    srun -N ${NODES} -n ${NODES} --drop-caches hostname  
	    srun -N ${NODES} -n ${NODES} --drop-caches rm -rf /tmp/myfifo-tmp/
	) >> $CLEANUPLOGFILE 2>> $CLEANUPERRLOGFILE
	
	echo "Running without LDCS ${LOOP}"
	echo "Running without LDCS ${LOOP}" >> $REPORTFILE
	
	
	TIMINGSFILE="logs/client_benchmark_wo_ldcs_${SPEC}_${LOOP}.timelog"
	LOGFILE="logs/client_benchmark_wo_ldcs_${SPEC}_${LOOP}.log"
	ERRLOGFILE="logs/client_benchmark_wo_ldcs_${SPEC}_${LOOP}.errlog"
	CMD="/usr/bin/time -o ${TIMINGSFILE} -v -v srun --distribution block -v -N ${NODES} -n ${TOTALTASKS} pynamic-pyMPI pynamic_driver.py"
	(
	    echo $CMD
	    $CMD
	) > $LOGFILE 2> $ERRLOGFILE
	grep 'Elapsed' ${TIMINGSFILE}
	grep 'Elapsed' ${TIMINGSFILE} >> $REPORTFILE

    fi


    if [ "$DO_LDCS" = "YES" ]
    then

	echo "Running Cleanup #${LOOP}"
	echo "Running Cleanup #${LOOP}" >> $REPORTFILE
	echo "Running Cleanup #${LOOP}" >> ${CLEANUPLOGFILE}
	echo "Running Cleanup #${LOOP}" >> ${CLEANUPERRLOGFILE}
	(
	    srun -N ${NODES} -n ${NODES} --drop-caches hostname  
	    srun -N ${NODES} -n ${NODES} --drop-caches rm -rf /tmp/myfifo-tmp/
	) >> $CLEANUPLOGFILE 2>> $CLEANUPERRLOGFILE

	let SERVERSIZE=${NODES}/${NUMSERVER}
	
	split -a 1 -d -l ${SERVERSIZE} ./tmp/hostlist_${SPEC}.dat ./tmp/hostlist_${SPEC}.dat_
	
	let NUMSERVERHELP=${NUMSERVER}-1
	for SUBLOOP in $(seq 0 ${NUMSERVERHELP})
	do
	    echo " Starting server in background #${LOOP} ${SUBLOOP}"
	    LOGFILE="logs/server_benchmark_${SPEC}_${LOOP}_${SUBLOOP}.log"
	    ERRLOGFILE="logs/server_benchmark_${SPEC}_${LOOP}_${SUBLOOP}.errlog"
	    MYH=`head -1 ./tmp/hostlist_${SPEC}.dat_${SUBLOOP}`
	    CMD="/g/g92/frings1/LLNL/LDCS/tools/cobo/test/server_rsh_ldcs -np ${SERVERSIZE} -paramfile /g/g92/frings1/LLNL/pynamic/benchmark3/tmp/cobo_param_bm_${SPEC}.dat -hostfile /g/g92/frings1/LLNL/pynamic/benchmark3/tmp/hostlist_${SPEC}.dat_${SUBLOOP} /g/g92/frings1/LLNL/LDCS/auditserver/ldcs_audit_server_par_pipe_md"
	    echo "cd $BMPATH; rsh ${MYH} ${CMD}"
	    echo "cd $BMPATH; rsh ${MYH} ${CMD}"  > $LOGFILE
	    rsh ${MYH} "(ulimit -n 2048;ulimit -a; ${CMD})" >> $LOGFILE 2> $ERRLOGFILE &
	    SERVERPID=$!
	    echo "SERVERPID=${SERVERPID}"
	done

	echo "sleep(${SLEEPTIME})"
	sleep ${SLEEPTIME}

	
	echo "Running with LDCS ${LOOP}"
	echo "Running with LDCS ${LOOP}" >> $REPORTFILE
	
	LDCS_NUMBER=7777 
	
	STARTFILE="tmp/client_benchmark_with_ldcs_${SPEC}_${LOOP}.sh"
	TIMINGSFILE="logs/client_benchmark_with_ldcs_${SPEC}_${LOOP}.timelog"
	LOGFILE="logs/client_benchmark_with_ldcs_${SPEC}_${LOOP}.log"
	ERRLOGFILE="logs/client_benchmark_with_ldcs_${SPEC}_${LOOP}.errlog"

	CMD="LDCS_LOCATION=/tmp/myfifo LDCS_NUMBER=7777 LD_AUDIT=/g/g92/frings1/LLNL/LDCS/auditclient/ldcs_audit_client_pipe.so srun --distribution block -v -N ${NODES} -n ${TOTALTASKS} pynamic-pyMPI pynamic_driver.py"
	echo $CMD > ${STARTFILE}
	chmod u+x ${STARTFILE}

	(
	    echo $CMD
	    /usr/bin/time -o ${TIMINGSFILE} -v ${STARTFILE}
	) > $LOGFILE 2> $ERRLOGFILE

	grep 'Elapsed' ${TIMINGSFILE}
	grep 'Elapsed' ${TIMINGSFILE} >> $REPORTFILE


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


	echo "Running Cleanup #${LOOP}"
	echo "Running Cleanup #${LOOP}" >> $REPORTFILE
	echo "Running Cleanup #${LOOP}" >> ${CLEANUPLOGFILE}
	echo "Running Cleanup #${LOOP}" >> ${CLEANUPERRLOGFILE}
	(
	    srun -N ${NODES} -n ${NODES} --drop-caches hostname  
	    srun -N ${NODES} -n ${NODES} --drop-caches rm -rf /tmp/myfifo-tmp/
	) >> $CLEANUPLOGFILE 2>> $CLEANUPERRLOGFILE

    fi
done