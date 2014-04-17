# $OpenBSD: Makefile.inc,v 1.1 2014/04/17 18:11:47 miod Exp $

# amd64-specific libcrypto build rules

SRCS+= bf_enc.c des_enc.c fcrypt_b.c
SRCS+= x86_64-gcc.c

CFLAGS+= -DOPENSSL_BN_ASM_GF2m
CFLAGS+= -DOPENSSL_BN_ASM_MONT
CFLAGS+= -DOPENSSL_BN_ASM_MONT5
CFLAGS+= -DOPENSSL_CPUID_OBJ
CFLAGS+= -DOPENSSL_IA32_SSE2

CFLAGS+= -DAES_ASM
CFLAGS+= -DBSAES_ASM
CFLAGS+= -DGHASH_ASM
CFLAGS+= -DMD5_ASM
CFLAGS+= -DSHA1_ASM
CFLAGS+= -DSHA256_ASM
CFLAGS+= -DSHA512_ASM
CFLAGS+= -DVPAES_ASM
CFLAGS+= -DWHIRLPOOL_ASM

SSLASM=\
	aes aes-x86_64 \
	aes aesni-x86_64 \
	aes aesni-sha1-x86_64 \
	aes bsaes-x86_64 \
	aes vpaes-x86_64 \
	bn x86_64-mont \
	bn x86_64-mont5 \
	bn x86_64-gf2m \
	bn modexp512-x86_64 \
	md5 md5-x86_64 \
	modes ghash-x86_64 \
	rc4 rc4-x86_64 \
	rc4 rc4-md5-x86_64 \
	sha sha1-x86_64 \
	whrlpool wp-x86_64

.for dir f in ${SSLASM}
SRCS+=	${f}.S
GENERATED+=${f}.S
${f}.S: ${LCRYPTO_SRC}/${dir}/asm/${f}.pl
	(cd ${LCRYPTO_SRC}/${dir} ; \
		/usr/bin/perl ./asm/${f}.pl openbsd-elf) > ${.TARGET}
.endfor

SRCS+=	x86_64cpuid.S sha256-x86_64.S sha512-x86_64.S
GENERATED+=x86_64cpuid.S sha256-x86_64.S sha512-x86_64.S

x86_64cpuid.S: ${LCRYPTO_SRC}/x86_64cpuid.pl
	(cd ${LCRYPTO_SRC}/${dir} ; \
		/usr/bin/perl ./x86_64cpuid.pl) > ${.TARGET}
sha256-x86_64.S: ${LCRYPTO_SRC}/sha/asm/sha512-x86_64.pl
	cd ${LCRYPTO_SRC}/sha/asm ; \
		/usr/bin/perl ./sha512-x86_64.pl ${.OBJDIR}/${.TARGET}
sha512-x86_64.S: ${LCRYPTO_SRC}/sha/asm/sha512-x86_64.pl
	cd ${LCRYPTO_SRC}/sha/asm ; \
		/usr/bin/perl ./sha512-x86_64.pl ${.OBJDIR}/${.TARGET}
