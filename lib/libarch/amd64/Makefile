#	$OpenBSD: Makefile,v 1.6 2008/05/23 15:38:34 jasper Exp $
#	$NetBSD: Makefile,v 1.1 1996/02/21 02:45:47 jtk Exp $

MANSUBDIR=amd64
MAN+=	amd64_iopl.2 amd64_get_ioperm.2 amd64_get_mtrr.2
MLINKS+=amd64_get_ioperm.2 amd64_set_ioperm.2 \
	amd64_get_mtrr.2 amd64_set_mtrr.2 

.if ${MACHINE_ARCH} == "amd64"
.PATH: ${LIBC}/amd64
NOPIC=
SRCS+=	amd64_iopl.c amd64_get_ioperm.c \
	amd64_set_ioperm.c amd64_set_mtrr.c amd64_get_mtrr.c
.include <bsd.lib.mk>
.else
NOPROG=
.include <bsd.prog.mk>
.endif

