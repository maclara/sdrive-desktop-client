# Mainly use this to show how to create an schroot
FROM opensuse/archive:42.3

MAINTAINER Markus Goetz <guruz@owncloud.com>

ENV TERM ansi
ENV HOME /root

ENV REFRESHED_AT 20170104

RUN zypper --no-gpg-checks --non-interactive --gpg-auto-import-keys refresh
RUN zypper --no-gpg-checks --non-interactive --gpg-auto-import-keys ar \
	http://download.opensuse.org/repositories/windows:/mingw/openSUSE_Leap_15.4/windows:mingw.repo
RUN zypper --no-gpg-checks --non-interactive --gpg-auto-import-keys ar \
	http://download.opensuse.org/repositories/isv:/ownCloud:/toolchains:/mingw:/win32:/2.3/openSUSE_Leap_42.1/isv:ownCloud:toolchains:mingw:win32:2.3.repo

RUN zypper --no-gpg-checks --non-interactive --gpg-auto-import-keys install cmake make \
	mingw32-cross-binutils mingw32-cross-cpp mingw32-cross-gcc mingw32-cross-gcc-c++ \
	mingw32-cross-pkg-config mingw32-filesystem mingw32-headers mingw32-runtime \
	site-config mingw32-cross-libqt5-qmake mingw32-cross-libqt5-qttools \
	mingw32-libqt5* mingw32-qt5keychain* mingw32-angleproject* mingw32-cross-nsis \
	mingw32-libopenssl* mingw32-sqlite* kdewin-png2ico osslsigncode wget || true

# mingw32-libwebp

# RPM depends on curl for installs from HTTP
RUN zypper --no-gpg-checks --non-interactive --gpg-auto-import-keys install curl || true

# sudo needed for building as user
RUN zypper --no-gpg-checks --non-interactive --gpg-auto-import-keys install sudo || true

# Use packaged UAC dependencies
RUN zypper --no-gpg-checks --non-interactive --gpg-auto-import-keys install mingw32-cross-nsis-plugin-uac mingw32-cross-nsis-plugin-nsprocess || true

# Required for checksumming
RUN zypper --no-gpg-checks --non-interactive --gpg-auto-import-keys install mingw32-zlib-devel || true

# Required for windres not to crash
RUN zypper --no-gpg-checks --non-interactive --gpg-auto-import-keys install glibc-locale || true

CMD /bin/bash
