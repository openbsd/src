#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	iostat
.if (${MACHINE_ARCH} == "m68k")
CFLAGS+=-D${MACHINE} -I${.CURDIR}/../../sys/arch
.endif
CFLAGS+=-I${.CURDIR}/../../usr.bin/vmstat
MAN=	iostat.8
DPADD=	${LIBKVM}
LDADD=	-lkvm
BINGRP=	kmem
BINMODE=2555

.include <bsd.prog.mk>
