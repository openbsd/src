#	$OpenBSD: Makefile,v 1.4 2001/07/02 18:13:23 deraadt Exp $

.if (${MACHINE} == "i386") || (${MACHINE} == "powerpc") || \
    (${MACHINE} == "alpha")
PROG=	ancontrol
CFLAGS+=-DANCACHE
.else
NOPROG=
.endif

MAN=	ancontrol.8

.include <bsd.prog.mk>
