# $OpenBSD: Makefile.inc,v 1.9 2023/04/15 18:23:54 tb Exp $

# i386-specific libcrypto build rules

# all i386 code generators use these
EXTRA_PL =  ${LCRYPTO_SRC}/perlasm/x86gas.pl ${LCRYPTO_SRC}/perlasm/x86asm.pl

# aes
CFLAGS+= -DAES_ASM
SSLASM+= aes aes-586
CFLAGS+= -DVPAES_ASM
SSLASM+= aes vpaes-x86
SSLASM+= aes aesni-x86
# bf
SRCS+= bf_cbc.c	
SSLASM+= bf bf-586
# bn
CFLAGS+= -DOPENSSL_IA32_SSE2
SSLASM+= bn bn-586
SSLASM+= bn co-586
CFLAGS+= -DOPENSSL_BN_ASM_MONT
SSLASM+= bn x86-mont
# camellia
SSLASM+= camellia cmll-x86
# des
SRCS+= fcrypt_b.c
SSLASM+= des des-586
# md5
CFLAGS+= -DMD5_ASM
SSLASM+= md5 md5-586
# modes
CFLAGS+= -DGHASH_ASM
SSLASM+= modes ghash-x86
# rc4
SSLASM+= rc4 rc4-586
# ripemd
CFLAGS+= -DRMD160_ASM
SSLASM+= ripemd rmd-586
# sha
CFLAGS+= -DSHA1_ASM
SSLASM+= sha sha1-586
CFLAGS+= -DSHA256_ASM
SSLASM+= sha sha256-586
CFLAGS+= -DSHA512_ASM
SSLASM+= sha sha512-586
# whrlpool
SRCS+= wp_block.c
CFLAGS+= -DWHIRLPOOL_ASM
SSLASM+= whrlpool wp-mmx

.for dir f in ${SSLASM}
SRCS+=	${f}.S
GENERATED+=${f}.S
${f}.S: ${LCRYPTO_SRC}/${dir}/asm/${f}.pl ${EXTRA_PL}
	/usr/bin/perl -I${LCRYPTO_SRC}/perlasm -I${LCRYPTO_SRC}/${dir}/asm \
		${LCRYPTO_SRC}/${dir}/asm/${f}.pl \
		    openbsd-elf ${CFLAGS} 386 ${PICFLAG} > ${.TARGET}
.endfor

CFLAGS+= -DOPENSSL_CPUID_OBJ
SRCS+=	x86cpuid.S
GENERATED+=x86cpuid.S

x86cpuid.S: ${LCRYPTO_SRC}/x86cpuid.pl ${EXTRA_PL}
	/usr/bin/perl -I${LCRYPTO_SRC}/perlasm ${LCRYPTO_SRC}/x86cpuid.pl \
		openbsd-elf ${CFLAGS} 386 ${PICFLAG} > ${.TARGET}
