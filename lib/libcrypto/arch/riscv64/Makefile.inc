# $OpenBSD: Makefile.inc,v 1.9 2024/03/29 06:03:21 jsing Exp $

# riscv64 libcrypto build rules

# camellia
SRCS+= camellia.c
SRCS+= cmll_cbc.c
SRCS+= cmll_misc.c
