#	$OpenBSD: Makefile,v 1.5 2004/02/28 23:51:35 deraadt Exp $
#	$NetBSD: Makefile,v 1.1 1996/02/21 02:45:47 jtk Exp $

MANSUBDIR=amd64
MAN+=	amd64_get_ldt.2 amd64_iopl.2 amd64_get_ioperm.2 amd64_get_mtrr.2
MLINKS+=amd64_get_ldt.2 amd64_set_ldt.2 \
	amd64_get_ioperm.2 amd64_set_ioperm.2 \
	amd64_get_mtrr.2 amd64_set_mtrr.2 

.if ${MACHINE_ARCH} == "amd64"
.PATH: ${LIBC}/amd64
NOPIC=
SRCS+=	amd64_get_ldt.c amd64_set_ldt.c amd64_iopl.c amd64_get_ioperm.c \
	amd64_set_ioperm.c amd64_set_mtrr.c amd64_get_mtrr.c
.include <bsd.lib.mk>
.else
NOPROG=
.include <bsd.prog.mk>
.endif

