#!/bin/sh

autoreconf -fiv
./configure --host=i686-w64-mingw32 --with-dllpath=/usr/i686-w64-mingw32/sys-root/mingw/bin/ --with-nxproto=3.5.0 --with-wxdir=/usr/i686-w64-mingw32/sys-root/mingw/bin/ --enable-staticwx CFLAGS="-static-libstdc++ -static-libgcc"  CXXFLAGS="-static-libstdc++ -static-libgcc" -with-wine-iscc="$HOME/.wine/drive_c/Program Files/Inno Setup 5/ISCC.exe" --prefix=/usr/i686-w64-mingw32/sys-root/mingw --with-nxwin --with-orignx
