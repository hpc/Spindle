#!/bin/bash

# Starts munged and the flux broker.
#
# For documentation on running Flux in containers, see
# https://flux-framework.readthedocs.io/en/latest/tutorials/containers

brokerOptions="-Scron.directory=/etc/flux/system/cron.d \
  -Stbon.fanout=256 \
  -Srundir=/run/flux \
  -Sstatedir=${STATE_DIRECTORY:-/var/lib/flux} \
  -Slocal-uri=local:///run/flux/local \
  -Slog-stderr-level=6 \
  -Slog-stderr-mode=local"

# Get the hostname that will resolve for the Docker bridge network.
address=$(echo $( nslookup "$( hostname -i )" | head -n 1 ))
parts=(${address//=/ })
hostName=${parts[2]}
thisHost=(${hostName//./ })
thisHost=${thisHost[0]}
echo $thisHost
export FLUX_FAKE_HOSTNAME=$thisHost

if [ -d /shared ]; then
    sudo chown -R "$(id -un):$(id -gn)" /shared
    sudo chmod 755 /shared
fi

# Start munged
sudo -u munge /usr/sbin/munged

if [ ${thisHost} != "${mainHost}" ]; then
    # Worker node -- wait for head node before connecting
    sleep 15
    FLUX_FAKE_HOSTNAME=$thisHost flux start -o --config /etc/flux/config ${brokerOptions} sleep inf
else
    # Head node
    FLUX_FAKE_HOSTNAME=$thisHost flux start -o --config /etc/flux/config ${brokerOptions} sleep inf
fi

