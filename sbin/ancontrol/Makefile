#	$OpenBSD: Makefile,v 1.6 2004/01/11 22:14:10 grange Exp $

.if (${MACHINE} == "i386") || (${MACHINE_ARCH} == "powerpc") || \
    (${MACHINE} == "alpha")
PROG=	ancontrol
CPPFLAGS+=-DANCACHE
.else
NOPROG=
.endif

MAN=	ancontrol.8

.include <bsd.prog.mk>
