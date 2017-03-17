#!/bin/bash

# Build windows version script

# Current dir: root of the build dir with unpacked files
# Args: NAME VERSION RELEASE DESTDIR
BUILDNAME=$1
VERSION=$2
RELEASE=$3
DESTDIR=$4

TARGETHOST=i586-pc-mingw32
MINGWPATH=/usr/$TARGETHOST/sys-root/mingw
WINCONFIGOPTIONS="--host $TARGETHOST"

# build source
./configure $WINCONFIGOPTIONS
jmake || exit

# build installer

# copying
mkdir -p $DESTDIR
cp -v rxclient*.exe $DESTDIR || exit

exit 0
