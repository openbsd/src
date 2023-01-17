# $OpenBSD: Makefile.inc,v 1.4 2023/01/17 15:04:27 miod Exp $

# powerpc-specific libcrypto build rules

# aes
SRCS+= aes_core.c aes_cbc.c 
# slower than C code
#CFLAGS+= -DAES_ASM
#SSLASM+= aes aes-ppc aes-ppc
# bf
SRCS+= bf_enc.c
# bn
SSLASM+= bn ppc bn-ppc
SSLASM+= bn ppc-mont ppc-mont		# bn_mul_mont_int
#SSLASM+= bn ppc64-mont ppc64-mont	# bn_mul_mont_fpu64
CFLAGS+= -DOPENSSL_BN_ASM_MONT
# camellia
SRCS+= camellia.c cmll_cbc.c cmll_misc.c
# des
SRCS+= des_enc.c fcrypt_b.c
# rc4
SRCS+= rc4_enc.c rc4_skey.c
# sha
CFLAGS+= -DSHA1_ASM
SSLASM+= sha sha1-ppc sha1-ppc
CFLAGS+= -DSHA256_ASM
SSLASM+= sha sha512-ppc sha256-ppc
# whrlpool
SRCS+= wp_block.c

.for dir src dst in ${SSLASM}
SRCS+=	${dst}.S
GENERATED+=${dst}.S
${dst}.S: ${LCRYPTO_SRC}/${dir}/asm/${src}.pl
	/usr/bin/perl \
		${LCRYPTO_SRC}/${dir}/asm/${src}.pl linux32 ${.TARGET} > ${.TARGET}
.endfor

#CFLAGS+= -DOPENSSL_CPUID_OBJ		# it's commented out in ppccap.c
SRCS+=	ppccap.c
