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

RUN yum install -y automake && git clone https://github.com/hpc/spindle && \
    cd spindle && \
    ./configure --with-munge-dir=/etc/munge --enable-sec-munge --with-slurm-dir=/etc/slurm --with-testrm=slurm && \
    make && make install

RUN groupadd spindle && \
    useradd --create-home --gid spindle spindle && \
    echo -n "spindle" | passwd --stdin spindle
