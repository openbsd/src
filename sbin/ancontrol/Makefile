#	$OpenBSD: Makefile,v 1.8 2004/01/29 03:42:49 deraadt Exp $

.if (${MACHINE} == "i386") || (${MACHINE} == "amd64") || \
    (${MACHINE_ARCH} == "powerpc") || (${MACHINE} == "alpha") || \
    (${MACHINE} == "cats")

PROG=	ancontrol
CPPFLAGS+=-DANCACHE
.else
NOPROG=
.endif

MAN=	ancontrol.8

.include <bsd.prog.mk>
