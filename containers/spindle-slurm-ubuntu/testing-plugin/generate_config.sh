#!/bin/bash

# Generate random password for the MariaDB slurm user
# and set it in config files

APP_USER=${APP_USER:-slurmuser}
MARIADB_PASS=$(openssl rand --base64 16 | head -c -3)
echo "MARIADB_PASSWORD: \"${MARIADB_PASS}\"" > mariadb.env
cp conf/slurmdbd.conf.template conf/slurmdbd.conf
echo "StoragePass=${MARIADB_PASS}" >> conf/slurmdbd.conf

# Enable Spindle SPANK plugin

echo "required /home/${APP_USER}/Spindle-inst/lib/libspindleslurm.so" > conf/plugstack.conf
