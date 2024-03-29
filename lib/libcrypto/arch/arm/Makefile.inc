# $oPenBSD: Makefile.inc,v 1.2 2014/05/02 18:21:39 miod Exp $

# arm-specific libcrypto build rules

# aes
CFLAGS+= -DAES_ASM
SSLASM+= aes aes-armv4
CFLAGS+= -DHAVE_AES_SET_ENCRYPT_KEY_INTERNAL
CFLAGS+= -DHAVE_AES_SET_DECRYPT_KEY_INTERNAL
CFLAGS+= -DHAVE_AES_ENCRYPT_INTERNAL
CFLAGS+= -DHAVE_AES_DECRYPT_INTERNAL
# bn
CFLAGS+= -DOPENSSL_BN_ASM_MONT
SSLASM+= bn armv4-mont
# modes
CFLAGS+= -DGHASH_ASM
SSLASM+= modes ghash-armv4
# sha
CFLAGS+= -DSHA1_ASM
SSLASM+= sha sha1-armv4-large
CFLAGS+= -DSHA256_ASM
SSLASM+= sha sha256-armv4
CFLAGS+= -DSHA512_ASM
SSLASM+= sha sha512-armv4

.for dir f in ${SSLASM}
SRCS+=	${f}.S
GENERATED+=${f}.S
${f}.S: ${LCRYPTO_SRC}/${dir}/asm/${f}.pl
	/usr/bin/perl \
		${LCRYPTO_SRC}/${dir}/asm/${f}.pl void ${.TARGET} > ${.TARGET}
.endfor

CFLAGS+= -DOPENSSL_CPUID_OBJ
SRCS+=	armv4cpuid.S armcap.c
