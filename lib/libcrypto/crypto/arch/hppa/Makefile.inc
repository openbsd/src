# $OpenBSD: Makefile.inc,v 1.8 2015/03/05 20:35:28 miod Exp $

# hppa-specific libcrypto build rules

# aes
SRCS+= aes_core.c aes_cbc.c 
CFLAGS+= -DAES_ASM
SSLASM+= aes aes-parisc aes-parisc
# bf
SRCS+= bf_enc.c
# bn
SRCS+= bn_asm.c
SSLASM+= bn parisc-mont parisc-mont
CFLAGS+= -DOPENSSL_BN_ASM_MONT -DBN_DIV2W
# camellia
SRCS+= camellia.c cmll_cbc.c cmll_misc.c
# des
SRCS+= des_enc.c fcrypt_b.c
# modes
CFLAGS+= -DGHASH_ASM
SSLASM+= modes ghash-parisc ghash-parisc
# rc4
.if 0	# about 35% slower than C code
SSLASM+= rc4 rc4-parisc rc4-parisc
.else
SRCS+= rc4_enc.c rc4_skey.c
.endif
## rc5
#SRCS+= rc5_enc.c 
# sha
CFLAGS+= -DSHA1_ASM
SSLASM+= sha sha1-parisc sha1-parisc
.if 0	# sha512 subtly broken, sha256 probably as well
CFLAGS+= -DSHA256_ASM
SSLASM+= sha sha512-parisc sha256-parisc
CFLAGS+= -DSHA512_ASM
SSLASM+= sha sha512-parisc sha512-parisc
.endif
# whrlpool
SRCS+= wp_block.c

.for dir src dst in ${SSLASM}
SRCS+=	${dst}.S
GENERATED+=${dst}.S
${dst}.S: ${LCRYPTO_SRC}/${dir}/asm/${src}.pl
	/usr/bin/perl \
		${LCRYPTO_SRC}/${dir}/asm/${src}.pl 32 ${.TARGET} > ${.TARGET}
.endfor

CFLAGS+= -DOPENSSL_CPUID_OBJ
SRCS+=	pariscid.S
GENERATED+=pariscid.S
pariscid.S: ${LCRYPTO_SRC}/pariscid.pl
	/usr/bin/perl \
		${LCRYPTO_SRC}/pariscid.pl 32 > ${.TARGET}
