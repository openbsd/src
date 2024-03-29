# $OpenBSD: Makefile.inc,v 1.13 2024/03/29 06:03:20 jsing Exp $

# alpha-specific libcrypto build rules

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
