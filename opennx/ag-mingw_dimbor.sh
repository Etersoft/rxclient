#!/bin/sh

autoreconf -fiv
./configure --host=i686-w64-mingw32 --with-dllpath=/usr/i686-w64-mingw32/bin \
    --with-nxproto=3.5.0 --with-wxdir=/usr/i686-w64-mingw32/bin \
    --enable-staticwx CFLAGS="-static-libstdc++ -static-libgcc" \
    CXXFLAGS="-static-libstdc++ -static-libgcc" \
    --with-wine-iscc=/usr/bin/iscc \
    --prefix=/usr/i686-w64-mingw32 --with-nxwin --with-orignx