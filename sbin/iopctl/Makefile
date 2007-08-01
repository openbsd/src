#	$OpenBSD: Makefile,v 1.9 2007/08/01 21:34:01 deraadt Exp $

.if (${MACHINE} == "i386") || (${MACHINE} == "amd64") || \
    (${MACHINE} == "macppc") || (${MACHINE} == "alpha") || \
    (${MACHINE} == "sgi")
PROG=   iopctl
.else
NOPROG=
.endif

MAN=	iopctl.8

.include <bsd.prog.mk>
