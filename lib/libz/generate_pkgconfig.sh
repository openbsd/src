#!/bin/sh
#
# $OpenBSD: generate_pkgconfig.sh,v 1.1 2011/05/04 07:36:38 jasper Exp $
#
# Generate pkg-config file for zlib.

usage() {
	echo "usage: ${0##*/} [-k] -c current_directory -o obj_directory"
	exit 1
}

curdir=
objdir=
while getopts "c:ko:" flag; do
	case "$flag" in
		c)
			curdir=$OPTARG
			;;
		o)
			objdir=$OPTARG
			;;
		*)
			usage
			;;
	esac
done

[ -n "${curdir}" ] || usage
if [ ! -d "${curdir}" ]; then
	echo "${0##*/}: ${curdir}: not found"
	exit 1
fi
[ -n "${objdir}" ] || usage
if [ ! -w "${objdir}" ]; then
	echo "${0##*/}: ${objdir}: not found or not writable"
	exit 1
fi

zlib_version=$(sed -n -e '/VERSION "/s/.*"\(.*\)".*/\1/p' < ${curdir}/zlib.h)

pc_file="${objdir}/zlib.pc"
cat > ${pc_file} << __EOF__
prefix=/usr
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: zlib
Description: zlib compression library
Version: ${zlib_version}
Requires: 
Libs: -L\${libdir} -lz
Cflags: -I\${includedir}
__EOF__
