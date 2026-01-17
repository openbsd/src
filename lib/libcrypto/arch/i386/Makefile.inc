# $OpenBSD: Makefile.inc,v 1.35 2026/01/17 16:18:32 jsing Exp $

# i386-specific libcrypto build rules

# all i386 code generators use these
EXTRA_PL =  ${LCRYPTO_SRC}/perlasm/x86gas.pl ${LCRYPTO_SRC}/perlasm/x86asm.pl

SRCS += crypto_cpu_caps.c

# aes
CFLAGS+= -DLIBRESSL_USE_AES_ASSEMBLY
SSLASM+= aes aes-586
SSLASM+= aes aesni-x86
SRCS += aes_i386.c

# bn
CFLAGS+= -DLIBRESSL_USE_BN_ASSEMBLY
SSLASM+= bn bn-586
SSLASM+= bn co-586
CFLAGS+= -DOPENSSL_BN_ASM_MONT
SSLASM+= bn x86-mont

# md5
CFLAGS+= -DLIBRESSL_USE_MD5_ASSEMBLY
SSLASM+= md5 md5-586

# modes
CFLAGS+= -DLIBRESSL_USE_GCM_ASSEMBLY
SSLASM+= modes ghash-x86
SRCS += gcm128_i386.c

# rc4
CFLAGS+= -DLIBRESSL_USE_RC4_ASSEMBLY
SSLASM+= rc4 rc4-586

# sha
CFLAGS+= -DLIBRESSL_USE_SHA_ASSEMBLY
SSLASM+= sha sha1-586
SSLASM+= sha sha256-586
SSLASM+= sha sha512-586

.for dir f in ${SSLASM}
SRCS+=	${f}.S
GENERATED+=${f}.S
${f}.S: ${LCRYPTO_SRC}/${dir}/asm/${f}.pl ${EXTRA_PL}
	/usr/bin/perl -I${LCRYPTO_SRC}/perlasm -I${LCRYPTO_SRC}/${dir}/asm \
		${LCRYPTO_SRC}/${dir}/asm/${f}.pl \
		    openbsd-elf ${CFLAGS} 386 ${PICFLAG} > ${.TARGET}
.endfor
