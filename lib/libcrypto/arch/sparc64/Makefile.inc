# $OpenBSD: Makefile.inc,v 1.22 2026/01/17 13:55:31 jsing Exp $

# sparc64-specific libcrypto build rules

# aes
SSLASM+= aes aes-sparcv9 aes-sparcv9
# bn
# modes
CFLAGS+= -DGHASH_ASM
SSLASM+= modes ghash-sparcv9 ghash-sparcv9
# sha
SSLASM+= sha sha1-sparcv9 sha1-sparcv9
SSLASM+= sha sha512-sparcv9 sha256-sparcv9
SSLASM+= sha sha512-sparcv9 sha512-sparcv9

.for dir src dst in ${SSLASM}
SRCS+=	${dst}.S
GENERATED+=${dst}.S
${dst}.S: ${LCRYPTO_SRC}/${dir}/asm/${src}.pl
	/usr/bin/env CC=${CC} /usr/bin/perl \
		${LCRYPTO_SRC}/${dir}/asm/${src}.pl ${.TARGET} -m64 > ${.TARGET}
.endfor
