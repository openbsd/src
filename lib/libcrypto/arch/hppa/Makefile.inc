# $OpenBSD: Makefile.inc,v 1.16 2024/03/27 12:54:42 jsing Exp $

# hppa-specific libcrypto build rules

# aes
SRCS+= aes_core.c aes_cbc.c 
CFLAGS+= -DAES_ASM
SSLASM+= aes aes-parisc aes-parisc
# bn
SSLASM+= bn parisc-mont parisc-mont
CFLAGS+= -DOPENSSL_BN_ASM_MONT -DBN_DIV2W
# camellia
SRCS+= camellia.c cmll_cbc.c cmll_misc.c
# des
SRCS+= des_enc.c fcrypt_b.c
# modes
CFLAGS+= -DGHASH_ASM
SSLASM+= modes ghash-parisc ghash-parisc
# rc4
.if 0	# about 35% slower than C code
SSLASM+= rc4 rc4-parisc rc4-parisc
.else
SRCS+= rc4.c
.endif
# sha
CFLAGS+= -DSHA1_ASM
SSLASM+= sha sha1-parisc sha1-parisc
CFLAGS+= -DSHA256_ASM
SSLASM+= sha sha512-parisc sha256-parisc
# whrlpool
SRCS+= wp_block.c

.for dir src dst in ${SSLASM}
SRCS+=	${dst}.S
GENERATED+=${dst}.S
${dst}.S: ${LCRYPTO_SRC}/${dir}/asm/${src}.pl
	/usr/bin/perl \
		${LCRYPTO_SRC}/${dir}/asm/${src}.pl 32 ${.TARGET} > ${.TARGET}
.endfor
