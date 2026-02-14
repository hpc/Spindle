ARG BASE_VERSION=latest
FROM ghcr.io/llnl/spindle-slurm-base:${BASE_VERSION}
ARG replicas=4
ENV workers=${replicas}
ENV TMPDIR=/tmp

ARG BUILD_ROOT=containers/spindle-slurm-ubuntu/testing-plugin

# Slurm daemons run as $SLURM_USER
ARG SLURM_USER=slurm

# Applications run as $USER
ARG USER=slurmuser
ARG UID=1001

# Set up the Slurm install already present in the base image
COPY ${BUILD_ROOT}/scripts/setup_slurm.sh /setup_slurm.sh
COPY ${BUILD_ROOT}/conf/slurm.conf /home/${SLURM_USER}/slurm.conf
COPY ${BUILD_ROOT}/conf/slurmdbd.conf /home/${SLURM_USER}/slurmdbd.conf
COPY ${BUILD_ROOT}/conf/cgroup.conf /home/${SLURM_USER}/cgroup.conf
RUN /setup_slurm.sh

USER ${USER}
WORKDIR /home/${USER}

# Copy the Spindle repo into the container and build it
RUN mkdir -p /home/${USER}/Spindle
COPY . /home/${USER}/Spindle
COPY ${BUILD_ROOT}/scripts/build_spindle.sh /home/${USER}/build_spindle.sh
RUN ./build_spindle.sh

USER root
COPY ${BUILD_ROOT}/scripts/setup_spank_plugin.sh /setup_spank_plugin.sh
COPY ${BUILD_ROOT}/conf/plugstack.conf /home/${SLURM_USER}/plugstack.conf
RUN /setup_spank_plugin.sh

USER ${USER}

COPY ${BUILD_ROOT}/scripts/entrypoint.sh /home/${USER}/entrypoint.sh
ENV PATH /home/${USER}/Spindle-inst/bin:$PATH

ENTRYPOINT /bin/bash ./entrypoint.sh

