#LAUNCHMON=/usr/global/tools/launchmon/chaos_5_x86_64_ib/launchmon-1.0.0-20120608
LAUNCHMON=/usr/global/tools/launchmon/chaos_5_x86_64_ib/launchmon-1.0.0-20120821
LTEST=${LAUNCHMON}/share/launchmon/tests

export LMON_PREFIX=/collab/usr/global/tools/launchmon/chaos_5_x86_64_ib/launchmon-1.0.0-20120821
export LMON_LAUNCHMON_ENGINE_PATH=/collab/usr/global/tools/launchmon/chaos_5_x86_64_ib/launchmon-1.0.0-20120821/bin/launchmon

MPI_JOB_LAUNCHER_PATH=/usr/bin/srun
export MPI_JOB_LAUNCHER_PATH

RM_TYPE=RC_slurm
export RM_TYPE

${LTEST}/fe_launch_smoketest ${LTEST}/simple_MPI 24 2 pdebug ${LTEST}/be_kicker
