# $OpenBSD: Makefile.inc,v 1.14 2024/11/08 13:34:24 jsing Exp $

# aarch64-specific libcrypto build rules

SRCS += crypto_cpu_caps.c

AFLAGS+= -mmark-bti-property
