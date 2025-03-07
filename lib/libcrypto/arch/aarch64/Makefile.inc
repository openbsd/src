# $OpenBSD: Makefile.inc,v 1.15 2025/03/07 14:21:22 jsing Exp $

# aarch64-specific libcrypto build rules

SRCS += crypto_cpu_caps.c

SRCS += sha256_aarch64.c
SRCS += sha256_aarch64_ce.S

AFLAGS+= -mmark-bti-property
