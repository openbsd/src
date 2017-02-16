#	$OpenBSD: Makefile.inc,v 1.2 2017/02/16 08:13:24 jsg Exp $

CFLAGS += -fpic -fno-stack-protector
CFLAGS += -march=armv8-a+nofp+nosimd # this is -msoft-float
AFLAGS += -D_STANDALONE
CPPFLAGS += -I${.CURDIR}/../../lib/libc/arch/aarch64
