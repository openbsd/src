# $OpenBSD: Makefile.inc,v 1.17 2026/01/17 16:18:31 jsing Exp $

# alpha-specific libcrypto build rules

# bn
CFLAGS+= -DLIBRESSL_USE_BN_ASSEMBLY
SSLASM+= bn alpha-mont
CFLAGS+= -DOPENSSL_BN_ASM_MONT

# modes
CFLAGS+= -DLIBRESSL_USE_GCM_ASSEMBLY
SSLASM+= modes ghash-alpha

# sha
CFLAGS+= -DLIBRESSL_USE_SHA_ASSEMBLY
SSLASM+= sha sha1-alpha

.for dir f in ${SSLASM}
SRCS+=	${f}.S
GENERATED+=${f}.S
${f}.S: ${LCRYPTO_SRC}/${dir}/asm/${f}.pl
	/usr/bin/perl \
		${LCRYPTO_SRC}/${dir}/asm/${f}.pl > ${.TARGET}
.endfor
