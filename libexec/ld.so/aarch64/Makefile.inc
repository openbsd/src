#	$OpenBSD: Makefile.inc,v 1.3 2017/08/26 16:26:17 kettenis Exp $

CFLAGS += -fpic
CFLAGS += -march=armv8-a+nofp+nosimd # this is -msoft-float
AFLAGS += -D_STANDALONE
CPPFLAGS += -I${.CURDIR}/../../lib/libc/arch/aarch64
