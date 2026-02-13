#!/usr/bin/env bash
set -euxo pipefail

cp /home/${SLURM_USER}/plugstack.conf /etc/slurm/plugstack.conf
chown -R slurm:slurm /etc/slurm
