# $OpenBSD: Makefile.inc,v 1.10 2024/03/28 12:52:58 jsing Exp $

# aarch64-specific libcrypto build rules

# aes
SRCS+= aes_core.c
# bn
# camellia
SRCS+= camellia.c cmll_cbc.c cmll_misc.c
# modes
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
