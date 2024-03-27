# $OpenBSD: Makefile.inc,v 1.4 2024/03/27 12:54:42 jsing Exp $

# riscv64 libcrypto build rules

# aes
SRCS+= aes_core.c
SRCS+= aes_cbc.c

# camellia
SRCS+= camellia.c
SRCS+= cmll_cbc.c
SRCS+= cmll_misc.c

# des
SRCS+= des_enc.c
SRCS+= fcrypt_b.c

# rc4
SRCS+= rc4.c

# whrlpool
SRCS+= wp_block.c
