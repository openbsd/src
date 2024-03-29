# $OpenBSD: Makefile.inc,v 1.21 2024/03/29 02:33:44 jsing Exp $

# hppa-specific libcrypto build rules

# aes
SRCS+= aes_core.c
CFLAGS+= -DAES_ASM
SSLASM+= aes aes-parisc aes-parisc
# bn
SSLASM+= bn parisc-mont parisc-mont
CFLAGS+= -DOPENSSL_BN_ASM_MONT -DBN_DIV2W
# camellia
SRCS+= camellia.c cmll_cbc.c cmll_misc.c
# modes
CFLAGS+= -DGHASH_ASM
SSLASM+= modes ghash-parisc ghash-parisc
# sha
CFLAGS+= -DSHA1_ASM
SSLASM+= sha sha1-parisc sha1-parisc
CFLAGS+= -DSHA256_ASM
SSLASM+= sha sha512-parisc sha256-parisc

.for dir src dst in ${SSLASM}
SRCS+=	${dst}.S
GENERATED+=${dst}.S
${dst}.S: ${LCRYPTO_SRC}/${dir}/asm/${src}.pl
	/usr/bin/perl \
		${LCRYPTO_SRC}/${dir}/asm/${src}.pl 32 ${.TARGET} > ${.TARGET}
.endfor
