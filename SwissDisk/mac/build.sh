#!/bin/bash -ex

SDIR="`pwd`/../.."
BDIR="`pwd`/build"

rm -rf "$BDIR"
mkdir -p "$BDIR"

cd "$BDIR"

QT_LOC="$HOME/Qt5.6.3/5.6.3/clang_64"

export PATH="$QT_LOC/bin:$PATH"
export PKG_CONFIG_PATH="/usr/local/opt/openssl@1.1/lib/pkgconfig"

cmake -DOEM_THEME_DIR="$SDIR/SwissDisk" -DCMAKE_INSTALL_PREFIX=../../install "$SDIR"

make -j6

make install

rm -rf ../../install/Library/Frameworks
mkdir -p ../../install/Library/Frameworks
cp -a $QT_LOC/lib/Qt{Sql,Widgets,Network,Xml,MacExtras,Gui,Core}.framework ../../install/Library/Frameworks/
rm -rf ../../install/Library/Frameworks/*.framework/Versions/5/Headers
rm -f ../../install/Library/Frameworks/*.framework/Versions/5/*_debug*

./admin/osx/create_mac.sh ../../install . 
#'3rd Party Mac Developer Installer: maClara, LLC (53R32TQWB6)'
