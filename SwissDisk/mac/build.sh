#!/bin/bash -ex

QT_LOC="/opt/homebrew/opt/qt@5"
QTK_LOC="/opt/homebrew/opt/qtkeychain"
SPARKLE="/opt/homebrew/Caskroom/sparkle/2.6.4"

SDIR="`pwd`/../.."
BDIR="`pwd`/build"
IDIR="`pwd`/install"
ADIR="$IDIR/SwissDisk.app/Contents"

rm -rf "$BDIR"
rm -rf "$IDIR"

mkdir -p "$BDIR"
cd "$BDIR"

export PATH="$QT_LOC/bin:$PATH"

cmake -DOEM_THEME_DIR="$SDIR/SwissDisk" -DCMAKE_INSTALL_PREFIX="$IDIR" \
	-DCMAKE_MODULE_PATH="$QTK_LOC/lib/cmake" -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_FRAMEWORK_PATH="$SPARKLE;$QT_LOC/lib" \
	"$SDIR"

make -j6

make install

codesign --sign - --force --preserve-metadata=entitlements,requirements,flags,runtime \
	--deep $ADIR/..

./admin/osx/create_mac.sh "$IDIR" .
