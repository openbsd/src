# $OpenBSD: Makefile.inc,v 1.8 2024/03/29 02:33:45 jsing Exp $

# riscv64 libcrypto build rules

# aes
SRCS+= aes_core.c

# camellia
SRCS+= camellia.c
SRCS+= cmll_cbc.c
SRCS+= cmll_misc.c
