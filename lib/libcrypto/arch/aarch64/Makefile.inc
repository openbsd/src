# $OpenBSD: Makefile.inc,v 1.1 2017/01/11 18:11:01 patrick Exp $

# aarch64-specific libcrypto build rules

# aes
SRCS+= aes_core.c aes_cbc.c
# bf
SRCS+= bf_enc.c
# bn
SRCS+= bn_asm.c
# camellia
SRCS+= camellia.c cmll_cbc.c cmll_misc.c
# des
SRCS+= des_enc.c fcrypt_b.c
# modes
# rc4
SRCS+= rc4_enc.c rc4_skey.c
# sha
# whrlpool
SRCS+= wp_block.c

.for dir f in ${SSLASM}
SRCS+=	${f}.S
GENERATED+=${f}.S
${f}.S: ${LCRYPTO_SRC}/${dir}/asm/${f}.pl
	/usr/bin/perl \
		${LCRYPTO_SRC}/${dir}/asm/${f}.pl void ${.TARGET} > ${.TARGET}
.endfor
