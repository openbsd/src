# $OpenBSD: Makefile.inc,v 1.7 2024/03/27 12:54:42 jsing Exp $

# aarch64-specific libcrypto build rules

# aes
SRCS+= aes_core.c aes_cbc.c
# bn
# camellia
SRCS+= camellia.c cmll_cbc.c cmll_misc.c
# des
SRCS+= des_enc.c fcrypt_b.c
# modes
# rc4
SRCS+= rc4.c
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
AFLAGS+= -mmark-bti-property
SRCS+=	arm64cpuid.S arm64cap.c
