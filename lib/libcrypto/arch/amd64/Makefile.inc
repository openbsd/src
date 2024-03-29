# $OpenBSD: Makefile.inc,v 1.27 2024/03/29 11:00:57 jsing Exp $

# amd64-specific libcrypto build rules

# all amd64 code generators use this
EXTRA_PL =	${LCRYPTO_SRC}/perlasm/x86_64-xlate.pl

# aes
CFLAGS+= -DAES_ASM
SSLASM+= aes aes-x86_64
CFLAGS+= -DBSAES_ASM
SSLASM+= aes bsaes-x86_64
CFLAGS+= -DVPAES_ASM
SSLASM+= aes vpaes-x86_64
SSLASM+= aes aesni-x86_64
CFLAGS+= -DHAVE_AES_CBC_ENCRYPT_INTERNAL
CFLAGS+= -DHAVE_AES_SET_ENCRYPT_KEY_INTERNAL
CFLAGS+= -DHAVE_AES_SET_DECRYPT_KEY_INTERNAL
CFLAGS+= -DHAVE_AES_ENCRYPT_INTERNAL
CFLAGS+= -DHAVE_AES_DECRYPT_INTERNAL
# bn
CFLAGS+= -DOPENSSL_IA32_SSE2
CFLAGS+= -DRSA_ASM
SSLASM+= bn modexp512-x86_64
CFLAGS+= -DOPENSSL_BN_ASM_MONT
SSLASM+= bn x86_64-mont
CFLAGS+= -DOPENSSL_BN_ASM_MONT5
SSLASM+= bn x86_64-mont5

# bn s2n-bignum
SRCS += bn_arch.c
SRCS += bignum_add.S
SRCS += bignum_cmadd.S
SRCS += bignum_cmul.S
SRCS += bignum_mul.S
SRCS += bignum_mul_4_8_alt.S
SRCS += bignum_mul_8_16_alt.S
SRCS += bignum_sqr.S
SRCS += bignum_sqr_4_8_alt.S
SRCS += bignum_sqr_8_16_alt.S
SRCS += bignum_sub.S
SRCS += word_clz.S

# md5
CFLAGS+= -DMD5_ASM
SSLASM+= md5 md5-x86_64
# modes
CFLAGS+= -DGHASH_ASM
SSLASM+= modes ghash-x86_64
# rc4
CFLAGS+= -DHAVE_RC4_INTERNAL
CFLAGS+= -DHAVE_RC4_SET_KEY_INTERNAL
SSLASM+= rc4 rc4-x86_64
# ripemd
# sha
CFLAGS+= -DSHA1_ASM
SSLASM+= sha sha1-x86_64
CFLAGS+= -DSHA256_ASM
SRCS+= sha256-x86_64.S
GENERATED+= sha256-x86_64.S
sha256-x86_64.S: ${LCRYPTO_SRC}/sha/asm/sha512-x86_64.pl ${EXTRA_PL}
	cd ${LCRYPTO_SRC}/sha/asm ; \
		/usr/bin/perl ./sha512-x86_64.pl ${.OBJDIR}/${.TARGET}
CFLAGS+= -DSHA512_ASM
SRCS+= sha512-x86_64.S
GENERATED+= sha512-x86_64.S
sha512-x86_64.S: ${LCRYPTO_SRC}/sha/asm/sha512-x86_64.pl ${EXTRA_PL}
	cd ${LCRYPTO_SRC}/sha/asm ; \
		/usr/bin/perl ./sha512-x86_64.pl ${.OBJDIR}/${.TARGET}

.for dir f in ${SSLASM}
SRCS+=	${f}.S
GENERATED+=${f}.S
${f}.S: ${LCRYPTO_SRC}/${dir}/asm/${f}.pl ${EXTRA_PL}
	(cd ${LCRYPTO_SRC}/${dir} ; \
		/usr/bin/perl ./asm/${f}.pl openbsd) > ${.TARGET}
.endfor

CFLAGS+= -DOPENSSL_CPUID_OBJ
SRCS+=	x86_64cpuid.S
GENERATED+=x86_64cpuid.S

x86_64cpuid.S: ${LCRYPTO_SRC}/x86_64cpuid.pl ${EXTRA_PL}
	(cd ${LCRYPTO_SRC}/${dir} ; \
		/usr/bin/perl ./x86_64cpuid.pl) > ${.TARGET}
