#!/bin/bash -e

SDIR="`pwd`/../.."
BDIR="`pwd`/build"

test -d "$BDIR" || mkdir -p "$BDIR"

# Assume we are running in the docker container

cd $BDIR

cmake -DOEM_THEME_DIR="$SDIR/SwissDisk" \
	-DCMAKE_TOOLCHAIN_FILE="$SDIR/admin/win/Toolchain-mingw32-openSUSE.cmake" \
	"$SDIR"

make -j12

"$SDIR/admin/win/download_runtimes.sh"

make package
