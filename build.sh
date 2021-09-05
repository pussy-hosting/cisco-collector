# try to build everything from scratch
# parameters are used for ./configure

aclocal
autoheader -f
autoconf --add-missing
automake --add-missing
autoreconf

./configure $@
make

#make install

