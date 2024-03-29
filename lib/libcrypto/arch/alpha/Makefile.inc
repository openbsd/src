# $OpenBSD: Makefile.inc,v 1.12 2024/03/29 02:33:44 jsing Exp $

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

.for dir f in ${SSLASM}
SRCS+=	${f}.S
GENERATED+=${f}.S
${f}.S: ${LCRYPTO_SRC}/${dir}/asm/${f}.pl
	/usr/bin/perl \
		${LCRYPTO_SRC}/${dir}/asm/${f}.pl > ${.TARGET}
.endfor
