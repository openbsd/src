#	$OpenBSD: Makefile,v 1.5 2004/01/28 17:45:22 deraadt Exp $

.if (${MACHINE} == "i386") || (${MACHINE} == "amd64") || \
    (${MACHINE} == "macppc") || (${MACHINE} == "pegasos") || \
    (${MACHINE} == "alpha")
PROG=   iopctl
.else
NOPROG=
.endif

MAN=	iopctl.8

.include <bsd.prog.mk>
