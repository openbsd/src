#	$OpenBSD: Makefile,v 1.4 2003/10/31 04:05:29 drahn Exp $

.if (${MACHINE} == "i386") || (${MACHINE} == "macppc") || \
    (${MACHINE} == "alpha") || (${MACHINE} == "pegasos")
PROG=   iopctl
.else
NOPROG=
.endif

MAN=	iopctl.8

.include <bsd.prog.mk>
