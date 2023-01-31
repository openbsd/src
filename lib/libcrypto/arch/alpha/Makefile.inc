# $OpenBSD: Makefile.inc,v 1.6 2023/01/31 06:17:10 jsing Exp $

# alpha-specific libcrypto build rules

# aes
SRCS+= aes_core.c aes_cbc.c 
# bf
SRCS+= bf_enc.c
# bn
SSLASM+= bn alpha-mont
CFLAGS+= -DOPENSSL_BN_ASM_MONT
# camellia
SRCS+= camellia.c cmll_cbc.c cmll_misc.c
# des
SRCS+= des_enc.c fcrypt_b.c
# modes
CFLAGS+= -DGHASH_ASM
SSLASM+= modes ghash-alpha
# rc4
SRCS+= rc4_enc.c rc4_skey.c
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
