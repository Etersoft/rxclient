
Building RXClient on Windows:
-----------------------------

First, install `msys2` with *mingw32* and *ucrt64* environments.

Packages required (env specific):
------------------------------------

all:

    msys/zip msys/vim

mingw32/msys2:

    mingw-w64-i686-autotools mingw-w64-i686-gcc mingw-w64-i686-wxwidgets3.0-msw

ucrt64/msys2:

    mingw-w64-ucrt-x86_64-autotools mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-wxwidgets3.0-msw


Building opennx on windows:
---------------------------

Determine packages required for building opennx. Use list above, and pick as
follows:

    * REQUIRED_PACKAGES = (all + env)

Once determined, proceed:

```bash
# Install dependencies
pacman -Sy {REQUIRED_PACKAGES}

# Working in opennx directory
cd PATH_TO_RXCLIENT_CHECKOUT/opennx

# Get Pulse Headers
mkdir pulse
cd pulse
wget http://unixforum.org/up/nxman/opennx/pa-win32-6.0-11.1.tar.gz
tar --wildcards -xzvf pa-win32-*.tar.gz '*.h'
cd ..

# Generate configure script
autoreconf -fvi

# Prepare build/install directories
mkdir -p build/install
cd build

# Configure build
../configure --with-nxproto=3.5.0 --enable-staticwx CFLAGS="-static-libstdc++ -static-libgcc"  CXXFLAGS="-static-libstdc++ -static-libgcc" --prefix=$(pwd)/install --with-nxwin --with-orignx

# Build
make -j$(( $(nproc) + 1 ))

# Install
make install

# Include env dll dependencies
cd install/bin
( dlls=($(strings rxclient.exe | grep -i '\.dll$' | grep ^lib)); for dll in ${dlls[@]}; do { cp -fv $(which $dll) .; }; done )
```

Finally, copy the following files/dirs from older builds into `install` directory
of new build as follows:

    bin/cyg*.dll -> install/bin
    bin/nx*.exe  -> install/bin
    bin/gspawn-win32-helper* -> install/bin
    share/fonts  -> install/share

Once files are copied, you can package `install` directory into new
distribution archive.
