# $OpenBSD: Makefile.inc,v 1.4 2014/11/17 20:31:21 miod Exp $

# amd64-specific libcrypto build rules

# aes
CFLAGS+= -DAES_ASM
SSLASM+= aes aes-x86_64
CFLAGS+= -DBSAES_ASM
SSLASM+= aes bsaes-x86_64
CFLAGS+= -DVPAES_ASM
SSLASM+= aes vpaes-x86_64
SSLASM+= aes aesni-x86_64
SSLASM+= aes aesni-sha1-x86_64
# bf
SRCS+= bf_enc.c
# bn
CFLAGS+= -DOPENSSL_IA32_SSE2
CFLAGS+= -DRSA_ASM
SSLASM+= bn modexp512-x86_64
CFLAGS+= -DOPENSSL_BN_ASM_MONT
SSLASM+= bn x86_64-mont
CFLAGS+= -DOPENSSL_BN_ASM_MONT5
SSLASM+= bn x86_64-mont5
CFLAGS+= -DOPENSSL_BN_ASM_GF2m
SSLASM+= bn x86_64-gf2m
# camellia
SRCS+=	cmll_misc.c
SSLASM+= camellia cmll-x86_64
# des
SRCS+= des_enc.c fcrypt_b.c
# md5
CFLAGS+= -DMD5_ASM
SSLASM+= md5 md5-x86_64
# modes
CFLAGS+= -DGHASH_ASM
SSLASM+= modes ghash-x86_64
# rc4
CFLAGS+= -DRC4_MD5_ASM
SSLASM+= rc4 rc4-x86_64
SSLASM+= rc4 rc4-md5-x86_64
# ripemd
# sha
CFLAGS+= -DSHA1_ASM
SSLASM+= sha sha1-x86_64
CFLAGS+= -DSHA256_ASM
SRCS+= sha256-x86_64.S
GENERATED+= sha256-x86_64.S
sha256-x86_64.S: ${LCRYPTO_SRC}/sha/asm/sha512-x86_64.pl
	cd ${LCRYPTO_SRC}/sha/asm ; \
		/usr/bin/perl ./sha512-x86_64.pl ${.OBJDIR}/${.TARGET}
CFLAGS+= -DSHA512_ASM
SRCS+= sha512-x86_64.S
GENERATED+= sha512-x86_64.S
sha512-x86_64.S: ${LCRYPTO_SRC}/sha/asm/sha512-x86_64.pl
	cd ${LCRYPTO_SRC}/sha/asm ; \
		/usr/bin/perl ./sha512-x86_64.pl ${.OBJDIR}/${.TARGET}
# whrlpool
CFLAGS+= -DWHIRLPOOL_ASM
SSLASM+= whrlpool wp-x86_64

.for dir f in ${SSLASM}
SRCS+=	${f}.S
GENERATED+=${f}.S
${f}.S: ${LCRYPTO_SRC}/${dir}/asm/${f}.pl
	(cd ${LCRYPTO_SRC}/${dir} ; \
		/usr/bin/perl ./asm/${f}.pl openbsd-elf) > ${.TARGET}
.endfor

CFLAGS+= -DOPENSSL_CPUID_OBJ
SRCS+=	x86_64cpuid.S x86_64-gcc.c
GENERATED+=x86_64cpuid.S

x86_64cpuid.S: ${LCRYPTO_SRC}/x86_64cpuid.pl
	(cd ${LCRYPTO_SRC}/${dir} ; \
		/usr/bin/perl ./x86_64cpuid.pl) > ${.TARGET}
