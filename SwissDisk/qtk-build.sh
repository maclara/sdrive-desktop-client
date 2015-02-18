#!/bin/bash

BDIR=qtk-build
rm -rf $BDIR
mkdir $BDIR
cd $BDIR

export PATH="$HOME/Qt5.6.3/5.6.3/clang_64/bin:$PATH"

cmake -DCMAKE_INSTALL_PREFIX=../install ../qtkeychain

make -j6

make install
