#!/usr/bin/env bash
set -euxo pipefail

mkdir -p /home/${USER}/Spindle-build
cd /home/${USER}/Spindle-build
/home/${USER}/Spindle/configure --prefix=/home/${USER}/Spindle-inst --enable-sec-munge --with-rm=slurm --with-rsh-launch --with-rsh-cmd=/usr/bin/ssh --with-cachepaths=/tmp/cachepath --with-commpath=/tmp/cachepath/commpath CFLAGS="-O2 -g" CXXFLAGS="-O2 -g"
make -j$(nproc)
make install

