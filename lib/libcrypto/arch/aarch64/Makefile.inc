# $OpenBSD: Makefile.inc,v 1.18 2026/01/17 16:18:31 jsing Exp $

# aarch64-specific libcrypto build rules

SRCS += crypto_cpu_caps.c

CFLAGS+= -DLIBRESSL_USE_SHA_ASSEMBLY
SRCS += sha1_aarch64.c
SRCS += sha1_aarch64_ce.S
SRCS += sha256_aarch64.c
SRCS += sha256_aarch64_ce.S
SRCS += sha512_aarch64.c
SRCS += sha512_aarch64_ce.S

AFLAGS+= -mmark-bti-property
