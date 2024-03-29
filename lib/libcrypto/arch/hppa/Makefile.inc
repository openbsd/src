# $OpenBSD: Makefile.inc,v 1.24 2024/03/29 11:00:57 jsing Exp $

# hppa-specific libcrypto build rules

# aes
CFLAGS+= -DAES_ASM
SSLASM+= aes aes-parisc aes-parisc
CFLAGS+= -DHAVE_AES_ENCRYPT_INTERNAL
CFLAGS+= -DHAVE_AES_DECRYPT_INTERNAL
# bn
SSLASM+= bn parisc-mont parisc-mont
CFLAGS+= -DOPENSSL_BN_ASM_MONT -DBN_DIV2W
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
