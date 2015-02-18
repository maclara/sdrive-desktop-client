#!/bin/bash -ex

SDIR="`pwd`/../.."
BDIR="`pwd`/build"

# Do not build with 10.10 until neon conflict is resolved
SDK="10.9"

rm -rf "$BDIR"
mkdir -p "$BDIR"

cd "$BDIR"

rm -rf "../install"

export PATH="$HOME/Qt5.6.3/5.6.3/clang_64/bin:$PATH"
export PKG_CONFIG_PATH="/usr/local/opt/openssl@1.1/lib/pkgconfig"

cmake -DOEM_THEME_DIR="$SDIR/SwissDisk" -DCMAKE_INSTALL_PREFIX=../../install "$SDIR"

make -j6

# make install

# ./admin/osx/create_mac.sh ../install . 'Developer ID Installer: Benjamin Collins (VJAD6F74WN)'
