# $Id: Makefile,v 1.2 2004/10/22 16:20:51 deraadt Exp $

.if (${MACHINE} == "i386") || (${MACHINE} == "amd64") || \
    (${MACHINE} == "alpha") || (${MACHINE} == "sgi") || \
    (${MACHINE} == "sparc64") || (${MACHINE_ARCH} == "powerpc") || \
    (${MACHINE} == "cats") || (${MACHINE} == "hppa")
PROG=	ipwcontrol
.else
NOPROG=
.endif

MAN=	ipwcontrol.8

.include <bsd.prog.mk>
