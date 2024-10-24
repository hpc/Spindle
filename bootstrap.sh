#!/bin/sh

echo Bootstrap in spindle
rm -rf autom4te.cache/
aclocal
autoheader
autoconf
automake --add-missing
automake

echo Bootstrap in client
cd src/client
rm -rf autom4te.cache/
aclocal
autoheader
autoconf
automake --add-missing
automake
cd ../..

echo Bootstrap in server
cd src/server
rm -rf autom4te.cache/
aclocal
autoheader
autoconf
automake --add-missing
automake
cd ../..

echo Bootstrap in fe
cd src/fe
rm -rf autom4te.cache/
aclocal
autoheader
autoconf
automake --add-missing
automake
cd ../..

automake
touch configure

