#!/usr/bin/env bash

echo "SLURM_ROLE: ${SLURM_ROLE}"

echo "Starting sshd..."
sudo bash -c 'ulimit -c unlimited; service ssh start'
echo "Starting munged..."
sudo -u munge /usr/sbin/munged

if [ -d /shared ]; then
    sudo chown -R "$(id -un):$(id -gn)" /shared
    sudo chmod 755 /shared
fi

if [ "${SLURM_ROLE}" = "db" ]; then
    echo "Starting slurmdbd..."
    sudo -u slurm /usr/sbin/slurmdbd -Dvvv
elif [ "${SLURM_ROLE}" = "ctl" ] ; then
    echo "Starting slurmctld..."
    sudo -u slurm /usr/sbin/slurmctld -i -Dvvv
elif [ "${SLURM_ROLE}" = "worker" ] ; then
    echo "Starting slurmd..."
    sudo bash -c 'ulimit -c unlimited; exec /usr/sbin/slurmd -Dvvv'
fi

sleep inf
