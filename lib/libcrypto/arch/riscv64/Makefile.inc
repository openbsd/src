# $OpenBSD: Makefile.inc,v 1.1 2023/07/07 15:51:03 jsing Exp $

# riscv64 libcrypto build rules

# aes
SRCS+= aes_core.c
SRCS+= aes_cbc.c 

# bf
SRCS+= bf_enc.c

# camellia
SRCS+= camellia.c
SRCS+= cmll_cbc.c
SRCS+= cmll_misc.c

# des
SRCS+= des_enc.c
SRCS+= fcrypt_b.c

# rc4
SRCS+= rc4_enc.c
SRCS+= rc4_skey.c

# whrlpool
SRCS+= wp_block.c
