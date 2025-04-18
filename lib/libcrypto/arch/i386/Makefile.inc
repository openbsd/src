# $OpenBSD: Makefile.inc,v 1.28 2025/04/18 13:19:39 jsing Exp $

# i386-specific libcrypto build rules

# all i386 code generators use these
EXTRA_PL =  ${LCRYPTO_SRC}/perlasm/x86gas.pl ${LCRYPTO_SRC}/perlasm/x86asm.pl

SRCS += crypto_cpu_caps.c

# aes
CFLAGS+= -DAES_ASM
SSLASM+= aes aes-586
SSLASM+= aes aesni-x86
# bn
CFLAGS+= -DOPENSSL_IA32_SSE2
SSLASM+= bn bn-586
SSLASM+= bn co-586
CFLAGS+= -DOPENSSL_BN_ASM_MONT
SSLASM+= bn x86-mont
# md5
CFLAGS+= -DMD5_ASM
SSLASM+= md5 md5-586
# modes
CFLAGS+= -DGHASH_ASM
SSLASM+= modes ghash-x86
# rc4
SSLASM+= rc4 rc4-586
# sha
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
