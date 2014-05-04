# $OpenBSD: Makefile.inc,v 1.1 2014/05/04 17:13:05 miod Exp $

# vax-specific libcrypto build rules

# aes
CFLAGS+=-DOPENSSL_NO_ASM
SRCS+= aes_core.c aes_cbc.c 
# bf
SRCS+= bf_enc.c
# bn
SRCS+= bn_asm_vax.S
# des
SRCS+= des_enc.c fcrypt_b.c
# rc4
SRCS+= rc4_enc.c rc4_skey.c
## rc5
#SRCS+= rc5_enc.c 
# whrlpool
SRCS+= wp_block.c
