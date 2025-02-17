#!/bin/bash -xe

if [ -d ../../debian ]; then
	(cd ../../ && fakeroot debian/rules clean)
fi

rm -rf ../../debian

cp -a debian ../../debian
cd ../../

fakeroot debian/rules binary-arch

fakeroot debian/rules clean

rm -rf debian
