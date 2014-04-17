# $OpenBSD: Makefile.inc,v 1.1 2014/04/17 18:11:47 miod Exp $

# i386-specific libcrypto build rules

SRCS+= wp_block.c
SRCS+= bf_cbc.c	

CFLAGS+= -DOPENSSL_BN_ASM_GF2m
CFLAGS+= -DOPENSSL_BN_ASM_MONT
CFLAGS+= -DOPENSSL_BN_ASM_PART_WORDS
CFLAGS+= -DOPENSSL_CPUID_OBJ
CFLAGS+= -DOPENSSL_IA32_SSE2

CFLAGS+= -DAES_ASM
CFLAGS+= -DGHASH_ASM
CFLAGS+= -DMD5_ASM
CFLAGS+= -DRMD160_ASM
CFLAGS+= -DSHA1_ASM
CFLAGS+= -DSHA256_ASM
CFLAGS+= -DSHA512_ASM
CFLAGS+= -DVPAES_ASM
CFLAGS+= -DWHIRLPOOL_ASM

SSLASM=\
	aes aes-586 \
	aes vpaes-x86 \
	aes aesni-x86 \
	bf bf-586 \
	bn bn-586 \
	bn co-586 \
	bn x86-mont \
	bn x86-gf2m \
	des crypt586 \
	des des-586 \
	md5 md5-586 \
	modes ghash-x86 \
	rc4 rc4-586 \
	ripemd rmd-586 \
	sha sha1-586 \
	sha sha256-586 \
	sha sha512-586 \
	whrlpool wp-mmx

.for dir f in ${SSLASM}
SRCS+=	${f}.S
GENERATED+=${f}.S
${f}.S: ${LCRYPTO_SRC}/${dir}/asm/${f}.pl ${LCRYPTO_SRC}/perlasm/x86gas.pl
	/usr/bin/perl -I${LCRYPTO_SRC}/perlasm -I${LCRYPTO_SRC}/${dir}/asm \
		${LCRYPTO_SRC}/${dir}/asm/${f}.pl \
		    openbsd-elf ${CFLAGS} 386 ${PICFLAG} > ${.TARGET}
.endfor

SRCS+=	x86cpuid.S
GENERATED+=x86cpuid.S

x86cpuid.S: ${LCRYPTO_SRC}/x86cpuid.pl ${LCRYPTO_SRC}/perlasm/x86gas.pl
	/usr/bin/perl -I${LCRYPTO_SRC}/perlasm ${LCRYPTO_SRC}/x86cpuid.pl \
		openbsd-elf ${CFLAGS} 386 ${PICFLAG} > ${.TARGET}
