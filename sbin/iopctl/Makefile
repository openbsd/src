#	$OpenBSD: Makefile,v 1.7 2004/05/03 15:18:19 drahn Exp $

.if (${MACHINE} == "i386") || (${MACHINE} == "amd64") || \
    (${MACHINE} == "macppc") || (${MACHINE} == "alpha") || \
    (${MACHINE} == "cats")
PROG=   iopctl
.else
NOPROG=
.endif

MAN=	iopctl.8

.include <bsd.prog.mk>
