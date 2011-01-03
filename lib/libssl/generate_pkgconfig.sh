#!/bin/sh
#
# $OpenBSD: generate_pkgconfig.sh,v 1.2 2011/01/03 09:32:01 jasper Exp $
#
# Generate pkg-config files for OpenSSL.

usage() {
	echo "usage: ${0##*/} [-k] -c current_directory -o obj_directory"
	exit 1
}

enable_krb5=false
curdir=
objdir=
while getopts "c:ko:" flag; do
	case "$flag" in
		c)
			curdir=$OPTARG
			;;
		k)
			enable_krb5=true
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
__EOF__
echo -n 'Libs: -L${libdir} -lcrypto ' >> ${pc_file}
echo '-lz' >> ${pc_file}
echo -n 'Cflags: -I${includedir} ' >> ${pc_file}
${enable_krb5} && echo -n '-I/usr/include/kerberosV' >> ${pc_file}
echo '' >> ${pc_file}


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
__EOF__
echo -n 'Libs: -L${libdir} -lssl -lcrypto ' >> ${pc_file}
echo '-lz' >> ${pc_file}
echo -n 'Cflags: -I${includedir} ' >> ${pc_file}
${enable_krb5} && echo -n '-I/usr/include/kerberosV' >> ${pc_file}
echo '' >> ${pc_file}


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
__EOF__
echo -n 'Libs: -L${libdir} -lssl -lcrypto ' >> ${pc_file}
echo '-lz' >> ${pc_file}
echo -n 'Cflags: -I${includedir} ' >> ${pc_file}
${enable_krb5} && echo -n '-I/usr/include/kerberosV' >> ${pc_file}
echo '' >> ${pc_file}
