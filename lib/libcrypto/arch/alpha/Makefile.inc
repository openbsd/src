# $OpenBSD: Makefile.inc,v 1.11 2024/03/28 12:52:58 jsing Exp $

# alpha-specific libcrypto build rules

# aes
SRCS+= aes_core.c
# bn
SSLASM+= bn alpha-mont
CFLAGS+= -DOPENSSL_BN_ASM_MONT
# camellia
SRCS+= camellia.c cmll_cbc.c cmll_misc.c
# modes
CFLAGS+= -DGHASH_ASM
SSLASM+= modes ghash-alpha
# sha
CFLAGS+= -DSHA1_ASM
SSLASM+= sha sha1-alpha
# whrlpool
SRCS+= wp_block.c

.for dir f in ${SSLASM}
SRCS+=	${f}.S
GENERATED+=${f}.S
${f}.S: ${LCRYPTO_SRC}/${dir}/asm/${f}.pl
	/usr/bin/perl \
		${LCRYPTO_SRC}/${dir}/asm/${f}.pl > ${.TARGET}
.endfor
