# $OpenBSD: Makefile.inc,v 1.3 2014/11/17 20:31:21 miod Exp $

# alpha-specific libcrypto build rules

# aes
SRCS+= aes_core.c aes_cbc.c 
# bf
SRCS+= bf_enc.c
# bn
SRCS+= bn_asm.c
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
## rc5
#SRCS+= rc5_enc.c 
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

CFLAGS+= -DOPENSSL_CPUID_OBJ
SRCS+=	alphacpuid.S
GENERATED+=alphacpuid.S
alphacpuid.S: ${LCRYPTO_SRC}/alphacpuid.pl
	/usr/bin/perl \
		${LCRYPTO_SRC}/alphacpuid.pl > ${.TARGET}
