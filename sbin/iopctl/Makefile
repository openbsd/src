#	$OpenBSD: Makefile,v 1.8 2004/08/06 23:06:23 deraadt Exp $

.if (${MACHINE} == "i386") || (${MACHINE} == "amd64") || \
    (${MACHINE} == "macppc") || (${MACHINE} == "alpha") || \
    (${MACHINE} == "cats") || (${MACHINE} == "sgi")
PROG=   iopctl
.else
NOPROG=
.endif

MAN=	iopctl.8

.include <bsd.prog.mk>
