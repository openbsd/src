# $OpenBSD: Makefile.inc,v 1.4 2014/12/07 15:45:44 miod Exp $

# mips64-specific libcrypto build rules

# aes
SRCS+= aes_cbc.c 
CFLAGS+= -DAES_ASM
SSLASM+= aes aes-mips aes-mips
# bf
SRCS+= bf_enc.c
# bn
.if ${MACHINE} == "sgi"	# because of R4000 support
SRCS+= bn_asm.c
.else
SSLASM+= bn mips bn-mips
CFLAGS+= -DBN_DIV3W
.endif
SSLASM+= bn mips-mont mips-mont
CFLAGS+= -DOPENSSL_BN_ASM_MONT
# camellia
SRCS+= camellia.c cmll_cbc.c cmll_misc.c
# des
SRCS+= des_enc.c fcrypt_b.c
# rc4
SRCS+= rc4_enc.c rc4_skey.c
## rc5
#SRCS+= rc5_enc.c 
# sha
SSLASM+= sha sha1-mips sha1-mips
CFLAGS+= -DSHA1_ASM
SSLASM+= sha sha512-mips sha256-mips
CFLAGS+= -DSHA256_ASM
SSLASM+= sha sha512-mips sha512-mips
CFLAGS+= -DSHA512_ASM
# whrlpool
SRCS+= wp_block.c

.for dir src dst in ${SSLASM}
SRCS+=	${dst}.S
GENERATED+=${dst}.S
${dst}.S: ${LCRYPTO_SRC}/${dir}/asm/${src}.pl
	/usr/bin/env CC=${CC} /usr/bin/perl \
		${LCRYPTO_SRC}/${dir}/asm/${src}.pl 64 ${.TARGET} > ${.TARGET}
.endfor
