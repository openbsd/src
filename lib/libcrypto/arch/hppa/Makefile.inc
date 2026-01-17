# $OpenBSD: Makefile.inc,v 1.30 2026/01/17 16:18:32 jsing Exp $

# hppa-specific libcrypto build rules

# aes
CFLAGS+= -DLIBRESSL_USE_AES_ASSEMBLY
SSLASM+= aes aes-parisc aes-parisc

# bn
CFLAGS+= -DLIBRESSL_USE_BN_ASSEMBLY
SSLASM+= bn parisc-mont parisc-mont
CFLAGS+= -DOPENSSL_BN_ASM_MONT

# modes
CFLAGS+= -DLIBRESSL_USE_GCM_ASSEMBLY
SSLASM+= modes ghash-parisc ghash-parisc

# sha
CFLAGS+= -DLIBRESSL_USE_SHA_ASSEMBLY
SSLASM+= sha sha1-parisc sha1-parisc
SSLASM+= sha sha512-parisc sha256-parisc

.for dir src dst in ${SSLASM}
SRCS+=	${dst}.S
GENERATED+=${dst}.S
${dst}.S: ${LCRYPTO_SRC}/${dir}/asm/${src}.pl
	/usr/bin/perl \
		${LCRYPTO_SRC}/${dir}/asm/${src}.pl 32 ${.TARGET} > ${.TARGET}
.endfor
