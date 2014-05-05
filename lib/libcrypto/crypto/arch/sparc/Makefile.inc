# $OpenBSD: Makefile.inc,v 1.1 2014/05/05 04:44:54 miod Exp $

# sparc-specific libcrypto build rules

# aes
SRCS+= aes_core.c aes_cbc.c 
# bf
SRCS+= bf_enc.c
# bn
.if 0 # uses `umul' and `udiv' instructions
SRCS+= sparcv8.S
.PATH: ${LCRYPTO_SRC}/bn/asm
.else
SRCS+= bn_asm.c
.endif
# des
SRCS+= fcrypt_b.c
SRCS+= des_enc-sparc.S
GENERATED+= des_enc-sparc.S
des_enc-sparc.S: ${LCRYPTO_SRC}/des/asm/des_enc.m4
	m4 ${LCRYPTO_SRC}/des/asm/des_enc.m4 > ${.TARGET}
# rc4
SRCS+= rc4_enc.c rc4_skey.c
## rc5
#SRCS+= rc5_enc.c 
# whrlpool
SRCS+= wp_block.c
