#	$OpenBSD: Makefile,v 1.3 2001/09/01 16:21:16 drahn Exp $

.if (${MACHINE} == "i386") || (${MACHINE} == "macppc") || \
    (${MACHINE} == "alpha")
PROG=   iopctl
.else
NOPROG=
.endif

MAN=	iopctl.8

.include <bsd.prog.mk>
