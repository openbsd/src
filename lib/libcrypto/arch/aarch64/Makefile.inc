# $OpenBSD: Makefile.inc,v 1.16 2025/03/12 14:13:41 jsing Exp $

# aarch64-specific libcrypto build rules

SRCS += crypto_cpu_caps.c

SRCS += sha256_aarch64.c
SRCS += sha256_aarch64_ce.S
SRCS += sha512_aarch64.c
SRCS += sha512_aarch64_ce.S

AFLAGS+= -mmark-bti-property
