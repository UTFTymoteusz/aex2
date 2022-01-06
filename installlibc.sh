#!/bin/sh

GCCPATH=$(which x86_64-pc-aex2-gcc)

if [ -z "$GCCPATH" ] 
then
    echo "failed to install the libc"
    exit 0
fi

VERSION=$(x86_64-pc-aex2-gcc --version | grep -o '[0-9]\+\.[0-9]\+\.[0-9]')
DIR=${GCCPATH%/*}
DST=$DIR/../lib/gcc/x86_64-pc-aex2/$VERSION/

cp -u  $DST
cp -ur include/ $DST