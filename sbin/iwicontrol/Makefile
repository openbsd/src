# $Id: Makefile,v 1.1 2004/10/20 12:50:48 deraadt Exp $

.if (${MACHINE} == "i386") || (${MACHINE} == "amd64") || \
    (${MACHINE} == "alpha") || (${MACHINE} == "sparc") || \
    (${MACHINE} == "sparc64") || (${MACHINE_ARCH} == "powerpc") || \
    (${MACHINE} == "cats") || (${MACHINE} == "hppa")
PROG=	iwicontrol
.else
NOPROG=
.endif

MAN=	iwicontrol.8

.include <bsd.prog.mk>
