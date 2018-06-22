#!/bin/sh

echo Bootstrap in spindle
autoheader
autoconf
aclocal
automake --add-missing
automake

echo Bootstrap in client
cd src/client
autoheader
autoconf
aclocal
automake --add-missing
automake
cd ../..

echo Bootstrap in server
cd src/server
autoheader
autoconf
aclocal
automake --add-missing
automake
cd ../..

echo Bootstrap in fe
cd src/fe
autoheader
autoconf
aclocal
automake --add-missing
automake
cd ../..

automake
touch configure

