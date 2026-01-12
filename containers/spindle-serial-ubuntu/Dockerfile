ARG ubuntu_version=noble
FROM ubuntu:${ubuntu_version}
USER root
ENV TMPDIR=/tmp
RUN echo 'TMPDIR="/tmp"' >> /etc/environment

RUN DEBIAN_FRONTEND="noninteractive" apt-get update \
# install latest pkg utils:
 && apt-get -qq install -y --no-install-recommends \
        apt-utils

RUN DEBIAN_FRONTEND="noninteractive" apt-get -qq install -y --no-install-recommends \
        locales \
        ca-certificates \
        wget \
        git \
        ssh \
        sudo \
        build-essential \
        pkg-config \
        autotools-dev \
        libtool \
        autoconf \
        automake \
        make \
        gfortran-13 \
        gcc-13 \
        g++-13 \
        munge \
        libmunge-dev \
        libhwloc-dev \
        mpich \
        libmpich-dev

# Prevent hwloc from trying to use graphics cards
# as this fails when X is not running.
ENV HWLOC_COMPONENTS=-gl

# Set up munge
RUN mkdir -p /run/munge && \
    chown munge:munge /run/munge && \
    chmod 0755 /run/munge

ARG USER=spindleuser
ARG UID=1001
ARG BUILD_ROOT=./containers/spindle-serial-ubuntu
COPY ${BUILD_ROOT}/scripts/add_docker_user.sh /add_docker_user.sh
RUN /add_docker_user.sh

USER ${USER}
WORKDIR /home/${USER}
RUN mkdir -p /home/${USER}/Spindle
# Copy the Spindle repo into the container
COPY . /home/${USER}/Spindle
COPY ${BUILD_ROOT}/scripts/build_spindle.sh /home/${USER}/build_spindle.sh
RUN ./build_spindle.sh

COPY ${BUILD_ROOT}/scripts/entrypoint.sh /home/${USER}/entrypoint.sh
ENV PATH /home/${USER}/Spindle-inst/bin:$PATH

ENTRYPOINT /bin/bash ./entrypoint.sh

