#	$OpenBSD: Makefile,v 1.3 2004/02/27 23:44:56 deraadt Exp $
#	$NetBSD: Makefile,v 1.1 1996/02/21 02:45:47 jtk Exp $

MAN+=	amd64_get_ldt.2 \
	amd64_iopl.2 amd64_get_ioperm.2
MANSUBDIR=amd64
MLINKS+=amd64_get_ldt.2 amd64_set_ldt.2 \
	amd64_get_ioperm.2 amd64_set_ioperm.2

.if ${MACHINE_ARCH} == "amd64"
.PATH: ${LIBC}/amd64
NOPIC=
SRCS+=	amd64_get_ldt.c amd64_set_ldt.c \
	amd64_iopl.c amd64_get_ioperm.c amd64_set_ioperm.c
.include <bsd.lib.mk>
.else
NOPROG=
.include <bsd.prog.mk>
.endif

