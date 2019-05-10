#	$OpenBSD: Makefile.inc,v 1.4 2019/05/10 13:29:21 guenther Exp $

CFLAGS += -fpic
CFLAGS += -march=armv8-a+nofp+nosimd # this is -msoft-float
AFLAGS += -D_STANDALONE
CPPFLAGS += -I${.CURDIR}/../../lib/libc/arch/aarch64
LD_SCRIPT = ${.CURDIR}/${MACHINE_CPU}/ld.script
