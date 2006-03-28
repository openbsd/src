#! /bin/sh

# Support for Debian GNU/NetBSD (netbsd-i386 and netbsd-alpha)
# A port of the Debian GNU system using the NetBSD kernel.

. ./hints/linux.sh

# Configure sets these where $osname = linux
ccdlflags='-Wl,-E'
lddlflags='-shared'
