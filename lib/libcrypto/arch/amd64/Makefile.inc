# $OpenBSD: Makefile.inc,v 1.46 2026/01/17 16:18:32 jsing Exp $

# amd64-specific libcrypto build rules

# all amd64 code generators use this
EXTRA_PL =	${LCRYPTO_SRC}/perlasm/x86_64-xlate.pl

SRCS += crypto_cpu_caps.c

# aes
CFLAGS+= -DLIBRESSL_USE_AES_ASSEMBLY
SSLASM+= aes aes-x86_64
SSLASM+= aes aesni-x86_64
SRCS += aes_amd64.c

# bn
SSLASM+= bn modexp512-x86_64
CFLAGS+= -DOPENSSL_BN_ASM_MONT
SSLASM+= bn x86_64-mont
CFLAGS+= -DOPENSSL_BN_ASM_MONT5
SSLASM+= bn x86_64-mont5

# bn s2n-bignum
CFLAGS+= -DLIBRESSL_USE_BN_ASSEMBLY
SRCS += bn_arch.c
SRCS += bignum_add.S
SRCS += bignum_cmadd.S
SRCS += bignum_cmul.S
SRCS += bignum_modadd.S
SRCS += bignum_modsub.S
SRCS += bignum_mul.S
SRCS += bignum_mul_4_8.S
SRCS += bignum_mul_4_8_alt.S
SRCS += bignum_mul_6_12.S
SRCS += bignum_mul_6_12_alt.S
SRCS += bignum_mul_8_16.S
SRCS += bignum_mul_8_16_alt.S
SRCS += bignum_sqr.S
SRCS += bignum_sqr_4_8.S
SRCS += bignum_sqr_4_8_alt.S
SRCS += bignum_sqr_6_12.S
SRCS += bignum_sqr_6_12_alt.S
SRCS += bignum_sqr_8_16.S
SRCS += bignum_sqr_8_16_alt.S
SRCS += bignum_sub.S
SRCS += word_clz.S

# md5
CFLAGS+= -DLIBRESSL_USE_MD5_ASSEMBLY
SRCS+= md5_amd64_generic.S

# modes
CFLAGS+= -DLIBRESSL_USE_GCM_ASSEMBLY
SSLASM+= modes ghash-x86_64
SRCS += gcm128_amd64.c

# rc4
CFLAGS+= -DLIBRESSL_USE_RC4_ASSEMBLY
SSLASM+= rc4 rc4-x86_64

# sha
CFLAGS+= -DLIBRESSL_USE_SHA_ASSEMBLY
SRCS+= sha1_amd64.c
SRCS+= sha1_amd64_generic.S
SRCS+= sha1_amd64_shani.S
SRCS+= sha256_amd64.c
SRCS+= sha256_amd64_generic.S
SRCS+= sha256_amd64_shani.S
SRCS+= sha512_amd64.c
SRCS+= sha512_amd64_generic.S

.for dir f in ${SSLASM}
SRCS+=	${f}.S
GENERATED+=${f}.S
${f}.S: ${LCRYPTO_SRC}/${dir}/asm/${f}.pl ${EXTRA_PL}
	(cd ${LCRYPTO_SRC}/${dir} ; \
		/usr/bin/perl ./asm/${f}.pl openbsd) > ${.TARGET}
.endfor

CFLAGS+=-fret-clean
