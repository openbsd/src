# $OpenBSD: Makefile.inc,v 1.34 2026/01/17 14:53:09 jsing Exp $

# i386-specific libcrypto build rules

# all i386 code generators use these
EXTRA_PL =  ${LCRYPTO_SRC}/perlasm/x86gas.pl ${LCRYPTO_SRC}/perlasm/x86asm.pl

SRCS += crypto_cpu_caps.c

# aes
SSLASM+= aes aes-586
SSLASM+= aes aesni-x86
SRCS += aes_i386.c

# bn
SSLASM+= bn bn-586
SSLASM+= bn co-586
CFLAGS+= -DOPENSSL_BN_ASM_MONT
SSLASM+= bn x86-mont

# md5
SSLASM+= md5 md5-586

# modes
SSLASM+= modes ghash-x86
SRCS += gcm128_i386.c

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
