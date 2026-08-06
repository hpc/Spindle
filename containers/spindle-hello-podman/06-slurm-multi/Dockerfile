FROM ubuntu:noble

USER root

# LC-specific fix for podman setgroups issue
ARG PODMAN_BUILD=false
RUN if [ "$PODMAN_BUILD" = "true" ]; then \
      echo 'APT::Sandbox::User root;' > /etc/apt/apt.conf.d/00-apt-sandbox; \
    fi

# Install build dependencies and runtime tools
# Build Slurm from source instead of using Ubuntu packages
# to avoid dpkg post-install chown failures in rootless podman
RUN apt-get update && \
    DEBIAN_FRONTEND="noninteractive" apt-get install -y --no-install-recommends \
        ca-certificates \
        build-essential \
        git \
        autoconf \
        automake \
        libtool \
        pkg-config \
        libmunge-dev \
        libmariadb-dev \
        libhwloc-dev \
        libjson-c-dev \
        libhttp-parser-dev \
        python3-dev \
        munge \
        sudo \
        iputils-ping \
 && rm -rf /var/lib/apt/lists/*

# Set up munge
RUN mkdir -p /run/munge /etc/munge && \
    chown munge:munge /run/munge && \
    chmod 0755 /run/munge

# Create munge key (same key on all nodes for simplicity)
RUN dd if=/dev/urandom bs=1 count=1024 > /etc/munge/munge.key && \
    chown munge:munge /etc/munge/munge.key && \
    chmod 400 /etc/munge/munge.key

# Create slurm user BEFORE building Slurm
RUN groupadd -r -g 900 slurm && \
    useradd -r -u 900 -g 900 -s /bin/false -d /var/lib/slurmd slurm

# Build Slurm from source
# Based on Spindle's build_slurm.sh
ARG SLURM_VERSION=slurm-25-05-3-1
RUN git clone -b ${SLURM_VERSION} --single-branch --depth=1 \
        https://github.com/SchedMD/slurm.git /tmp/slurm && \
    cd /tmp/slurm && \
    ./configure \
        --prefix=/usr \
        --sysconfdir=/etc/slurm \
        --with-mysql_config=/usr/bin \
        --libdir=/usr/lib && \
    make -j$(nproc) && \
    make install && \
    cd / && \
    rm -rf /tmp/slurm

# Create Slurm directories with correct ownership
# Based on Spindle's setup_slurm.sh
RUN mkdir -p \
        /etc/slurm \
        /var/spool/slurmd \
        /var/spool/slurmctld \
        /var/run/slurmd \
        /var/lib/slurmd \
        /var/log/slurm && \
    chown -R slurm:slurm \
        /etc/slurm \
        /var/spool/slurmd \
        /var/spool/slurmctld \
        /var/run/slurmd \
        /var/lib/slurmd \
        /var/log/slurm

# Copy Slurm configuration
COPY containers/spindle-hello-podman/06-slurm-multi/slurm.conf /etc/slurm/slurm.conf
COPY containers/spindle-hello-podman/06-slurm-multi/cgroup.conf /etc/slurm/cgroup.conf
RUN chown slurm:slurm /etc/slurm/slurm.conf /etc/slurm/cgroup.conf

# Copy entrypoint
COPY containers/spindle-hello-podman/06-slurm-multi/entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]
