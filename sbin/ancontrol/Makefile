#	$OpenBSD: Makefile,v 1.7 2004/01/28 17:45:21 deraadt Exp $

.if (${MACHINE} == "i386") || (${MACHINE} == "amd64") || \
    (${MACHINE_ARCH} == "powerpc") || (${MACHINE} == "alpha")
PROG=	ancontrol
CPPFLAGS+=-DANCACHE
.else
NOPROG=
.endif

MAN=	ancontrol.8

.include <bsd.prog.mk>
