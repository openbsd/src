#!/bin/sh
#
# $OpenBSD: generate_pkgconfig.sh,v 1.3 2011/01/21 09:24:46 jasper Exp $
#
# Generate pkg-config files for OpenSSL.

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
if [ ! -w "${curdir}" ]; then
	echo "${0##*/}: ${curdir}: not found or not writable"
	exit 1
fi
[ -n "${objdir}" ] || usage
if [ ! -w "${objdir}" ]; then
	echo "${0##*/}: ${objdir}: not found or not writable"
	exit 1
fi

ssl_version=$(sed -nE 's/^#define[[:blank:]]+SHLIB_VERSION_NUMBER[[:blank:]]+"(.*)".*/\1/p' \
	${curdir}/src/crypto/opensslv.h)

pc_file="${objdir}/libcrypto.pc"
cat > ${pc_file} << __EOF__
prefix=/usr
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: OpenSSL-libcrypto
Description: OpenSSL cryptography library
Version: ${ssl_version}
Requires: 
Libs: -lcrypto
Cflags: 
__EOF__


pc_file="${objdir}/libssl.pc"
cat > ${pc_file} << __EOF__
prefix=/usr
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: OpenSSL
Description: Secure Sockets Layer and cryptography libraries
Version: ${ssl_version}
Requires: 
Libs: -lssl -lcrypto
Cflags: 
__EOF__


pc_file="${objdir}/openssl.pc"
cat > ${pc_file} << __EOF__
prefix=/usr
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: OpenSSL
Description: Secure Sockets Layer and cryptography libraries and tools
Version: ${ssl_version}
Requires: 
Libs: -lssl -lcrypto
Cflags: 
__EOF__
