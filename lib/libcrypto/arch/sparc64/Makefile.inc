# $OpenBSD: Makefile.inc,v 1.24 2026/01/17 16:18:32 jsing Exp $

# sparc64-specific libcrypto build rules

# aes
CFLAGS+= -DLIBRESSL_USE_AES_ASSEMBLY
SSLASM+= aes aes-sparcv9 aes-sparcv9

# modes
CFLAGS+= -DLIBRESSL_USE_GCM_ASSEMBLY
SSLASM+= modes ghash-sparcv9 ghash-sparcv9

# sha
CFLAGS+= -DLIBRESSL_USE_SHA_ASSEMBLY
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
