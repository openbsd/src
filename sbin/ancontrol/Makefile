#	$OpenBSD: Makefile,v 1.9 2004/08/06 20:33:02 mickey Exp $

.if (${MACHINE} == "i386") || (${MACHINE} == "amd64") || \
    (${MACHINE_ARCH} == "powerpc") || (${MACHINE} == "alpha") || \
    (${MACHINE} == "cats") || (${MACHINE} == "hppa")

PROG=	ancontrol
CPPFLAGS+=-DANCACHE
.else
NOPROG=
.endif

MAN=	ancontrol.8

.include <bsd.prog.mk>
