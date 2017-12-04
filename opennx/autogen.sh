#! /bin/sh

autoreconf -fiv
./configure --enable-warn --prefix=/usr $*
