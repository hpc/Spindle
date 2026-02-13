#!/usr/bin/env bash

echo "SLURM_ROLE: ${SLURM_ROLE}"

echo "Starting sshd..."
sudo service ssh start
echo "Starting munged..."
sudo -u munge /usr/sbin/munged 

if [ "${SLURM_ROLE}" = "db" ]; then
    echo "Starting slurmdbd..."
    sudo -u slurm /usr/sbin/slurmdbd -Dvvv
elif [ "${SLURM_ROLE}" = "ctl" ] ; then
    echo "Starting slurmctld..."
    sudo -u slurm /usr/sbin/slurmctld -i -Dvvv
elif [ "${SLURM_ROLE}" = "worker" ] ; then
    echo "Starting slurmd..."
    sudo /usr/sbin/slurmd -Dvvv
fi

sleep inf
