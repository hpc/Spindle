#!/usr/bin/env bash
set -euxo pipefail

mkdir -p /home/${USER}/Spindle-build
cd /home/${USER}/Spindle-build
/home/${USER}/Spindle/configure --prefix=/home/${USER}/Spindle-inst --enable-sec-munge --with-rm=slurm-plugin --enable-slurm-plugin --with-commpath=/tmp --with-cachepaths=/tmp CFLAGS="-O2 -g" CXXFLAGS="-O2 -g"
make -j$(nproc)
make install

