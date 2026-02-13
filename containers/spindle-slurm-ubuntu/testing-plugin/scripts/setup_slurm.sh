#!/usr/bin/env bash
set -euxo pipefail

mkdir -p /etc/slurm /etc/sysconfig/slurm /var/spool/slurmd /var/spool/slurmctld /var/run/slurmd /var/run/slurmdbd /var/lib/slurmd /var/log/slurm
touch /var/lib/slurmd/node_state /var/lib/slurmd/front_end_state /var/lib/slurmd/job_state /var/lib/slurmd/resv_state /var/lib/slurmd/trigger_state /var/lib/slurmd/assoc_mgr_state /var/lib/slurmd/assoc_usage /var/lib/slurmd/qos_usage /var/lib/slurmd/fed_mgr_state 
cp /home/${SLURM_USER}/slurm.conf /etc/slurm/slurm.conf
cp /home/${SLURM_USER}/slurmdbd.conf /etc/slurm/slurmdbd.conf
cp /home/${SLURM_USER}/cgroup.conf /etc/slurm/cgroup.conf
chown -R slurm:slurm /etc/slurm /etc/sysconfig/slurm /var/spool/slurmd /var/spool/slurmctld /var/run/slurmd /var/run/slurmdbd /var/lib/slurmd /var/log/slurm 
chmod 600 /etc/slurm/slurmdbd.conf
