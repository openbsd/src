#	$OpenBSD: Makefile,v 1.6 2004/01/29 02:55:47 deraadt Exp $

.if (${MACHINE} == "i386") || (${MACHINE} == "amd64") || \
    (${MACHINE} == "macppc") || (${MACHINE} == "pegasos") || \
    (${MACHINE} == "alpha") || (${MACHINE} == "cats")
PROG=   iopctl
.else
NOPROG=
.endif

MAN=	iopctl.8

.include <bsd.prog.mk>
