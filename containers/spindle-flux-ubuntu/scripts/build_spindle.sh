#!/bin/bash

set -euxo pipefail

mkdir -p /home/${USER}/Spindle-build
cd /home/${USER}/Spindle-build
/home/${USER}/Spindle/configure --prefix=/home/${USER}/Spindle-inst --enable-sec-munge --with-rm=flux --enable-flux-plugin --with-cachepaths=/tmp/commpath/cachepath --with-commpath=/tmp/commpath CFLAGS="-O2 -g" CXXFLAGS="-O2 -g"
make -j$(nproc)
make install

