# $OpenBSD: Makefile.inc,v 1.17 2024/03/29 11:00:57 jsing Exp $

# mips64-specific libcrypto build rules

# aes
CFLAGS+= -DAES_ASM
SSLASM+= aes aes-mips aes-mips
CFLAGS+= -DHAVE_AES_SET_ENCRYPT_KEY_INTERNAL
CFLAGS+= -DHAVE_AES_SET_DECRYPT_KEY_INTERNAL
CFLAGS+= -DHAVE_AES_ENCRYPT_INTERNAL
CFLAGS+= -DHAVE_AES_DECRYPT_INTERNAL
# bn
SSLASM+= bn mips bn-mips
SSLASM+= bn mips-mont mips-mont
CFLAGS+= -DOPENSSL_BN_ASM_MONT
# sha
SSLASM+= sha sha1-mips sha1-mips
CFLAGS+= -DSHA1_ASM
SSLASM+= sha sha512-mips sha256-mips
CFLAGS+= -DSHA256_ASM
SSLASM+= sha sha512-mips sha512-mips
CFLAGS+= -DSHA512_ASM

.for dir src dst in ${SSLASM}
SRCS+=	${dst}.S
GENERATED+=${dst}.S
${dst}.S: ${LCRYPTO_SRC}/${dir}/asm/${src}.pl
	/usr/bin/env CC=${CC} /usr/bin/perl \
		${LCRYPTO_SRC}/${dir}/asm/${src}.pl 64 ${.TARGET} > ${.TARGET}
.endfor
