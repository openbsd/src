# $OpenBSD: Makefile.inc,v 1.7 2024/03/28 12:52:58 jsing Exp $

# riscv64 libcrypto build rules

# aes
SRCS+= aes_core.c

# camellia
SRCS+= camellia.c
SRCS+= cmll_cbc.c
SRCS+= cmll_misc.c

# whrlpool
SRCS+= wp_block.c
