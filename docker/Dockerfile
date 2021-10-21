FROM vanessa/slurm:18.08.6

LABEL org.label-schema.vcs-url="https://github.com/hpc/spindle" \
      org.label-schema.docker.cmd="docker-compose up -d" \
      org.label-schema.name="spindle" \
      org.label-schema.description="Spindle with SLURM on Centos 7" \
      maintainer="Vanessa Sochat"

# Install ompi
RUN wget https://www.open-mpi.org/software/ompi/v1.10/downloads/openmpi-1.10.2.tar.gz && \
    tar -xzvf openmpi-1.10.2.tar.gz && cd openmpi-1.10.2/ && \
    ./configure --with-slurm --prefix="/home/$USER/.openmpi" && \    
    make && make install

ENV PATH "$PATH:/home/$USER/.openmpi/bin"
ENV LD_LIBRARY_PATH "$LD_LIBRARY_PATH:/home/$USER/.openmpi/lib/" 

ENV TMPDIR /tmp

RUN yum install -y nc openssh-server openssh-clients iputils automake && git clone https://github.com/hpc/spindle && \
    cd spindle && \
#    --with-rsh-launch-option --with-testrm=slurm
#    ./configure --with-munge-dir=/etc/munge --enable-sec-munge --with-slurm-dir=/etc/slurm --with-rsh-launch-option && \
    ./configure --with-testrm=slurm --with-rsh-launch --with-rsh-cmd=/usr/bin/ssh --with-tmpdir=/tmp && \
    make && make install

RUN mkdir -p /var/run/sshd && \
    chmod 0755 /var/run/sshd && \
    useradd -p spindle --create-home --shell /bin/bash spindle

RUN ssh-keygen -b 2048 -t rsa -f /root/.ssh/id_rsa -q -N "" && \
    su spindle -c "ssh-keygen -b 2048 -t rsa -f /home/spindle/.ssh/id_rsa -q -N ''" && \
    cat ~/.ssh/id_rsa.pub >> ~/.ssh/authorized_keys && \
    cat /home/spindle/.ssh/id_rsa.pub >> /home/spindle/.ssh/authorized_keys

# Put spindle n debug mode, can be 1,2,3 (or unset)
ENV SPINDLE_DEBUG 3
EXPOSE 22

COPY docker-entrypoint.sh /docker-entrypoint.sh
ENTRYPOINT ["/docker-entrypoint.sh"]
