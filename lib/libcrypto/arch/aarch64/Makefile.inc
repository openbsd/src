# $OpenBSD: Makefile.inc,v 1.3 2022/03/23 15:13:31 tb Exp $

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

CFLAGS+= -DOPENSSL_CPUID_OBJ
SRCS+=	arm64cpuid.S arm64cap.c
