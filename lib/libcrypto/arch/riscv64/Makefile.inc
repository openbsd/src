# $OpenBSD: Makefile.inc,v 1.5 2024/03/28 01:41:16 jsing Exp $

# riscv64 libcrypto build rules

# aes
SRCS+= aes_core.c
SRCS+= aes_cbc.c

# camellia
SRCS+= camellia.c
SRCS+= cmll_cbc.c
SRCS+= cmll_misc.c

# rc4
SRCS+= rc4.c

# whrlpool
SRCS+= wp_block.c
