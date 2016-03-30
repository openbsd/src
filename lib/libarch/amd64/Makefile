#	$OpenBSD: Makefile,v 1.13 2016/03/30 06:38:41 jmc Exp $
#	$NetBSD: Makefile,v 1.1 1996/02/21 02:45:47 jtk Exp $

MANSUBDIR=amd64
MAN+=	amd64_iopl.2 amd64_get_fsbase.2

.if ${MACHINE_ARCH} == "amd64"
.PATH: ${LIBC}/amd64
NOPIC=
SRCS+=	amd64_iopl.c amd64_get_fsbase.c amd64_set_fsbase.c
.include <bsd.lib.mk>
.else
NOPROG=
.include <bsd.prog.mk>
.endif
