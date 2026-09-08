#!/bin/sh
#
# $OpenBSD: generate_pkgconfig.sh,v 1.1 2026/09/08 19:15:21 kirill Exp $
#
# Copyright (c) 2010,2011 Jasper Lievisse Adriaanse <jasper@openbsd.org>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#
# Generate pkg-config files for OpenSSL.

usage() {
	echo "usage: ${0##*/} -c current_directory -o obj_directory"
	exit 1
}

curdir=
objdir=
while getopts "c:o:" flag; do
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

version_re='1s/^\* Version ([0-9]+\.[0-9]+\.[0-9]+) .*/\1/p'
version_file=${curdir}/NEWS
lib_version=$(sed -nE "${version_re}" "${version_file}")

pc_file="${objdir}/libfido2.pc"
cat > ${pc_file} << __EOF__
prefix=/usr
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: libfido2
Description: A FIDO2 library
URL: https://github.com/yubico/libfido2
Version: ${lib_version}
Requires: libcrypto
Libs: -L\${libdir} -lfido2
Cflags: -I\${includedir}
__EOF__
