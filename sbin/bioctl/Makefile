#	$OpenBSD: Makefile,v 1.1 2005/03/29 22:04:21 marco Exp $

.if (${MACHINE} == "i386") || (${MACHINE} == "amd64") || \
    (${MACHINE_ARCH} == "powerpc") || (${MACHINE} == "alpha") || \
    (${MACHINE} == "cats") || (${MACHINE} == "hppa")

PROG=	bioctl
.else
NOPROG=
.endif

MAN=	bioctl.8

.include <bsd.prog.mk>
