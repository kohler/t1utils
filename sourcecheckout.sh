#! /bin/bash

usage () {
    echo "Usage: ./sourcecheckout.sh" 1>&2
    exit 1
}

if [ ! -f t1asm.c ]; then
    echo "sourcecheckout.sh must be run from the top t1utils source directory" 1>&2
    usage
fi
if [ ! -f ../liblcdf/include/lcdf/clp.h ]; then
    echo "sourcecheckout.sh can't find ../liblcdf" 1>&2
    usage
fi

test -d include || mkdir include
test -d include/lcdf || mkdir include/lcdf
for f in clp.h inttypes.h; do
    ln -sf ../../../liblcdf/include/lcdf/$f include/lcdf/$f
done
for f in clp.c strerror.c; do
    ln -sf ../liblcdf/liblcdf/$f $f
done
