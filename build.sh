#!/bin/sh
# try to build everything from scratch
# parameters are used for ./configure

aclocal
autoheader -f
autoconf
automake --add-missing
autoreconf -f

# shellcheck disable=SC2068
./configure $@
make

#make install

