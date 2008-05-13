#	$OpenBSD: Makefile,v 1.3 2008/05/13 21:12:44 miod Exp $

.if (${MACHINE} == "alpha") || (${MACHINE} == "amd64") || \
    (${MACHINE} == "armish") || (${MACHINE} == "hppa") || \
    (${MACHINE} == "hppa64") || (${MACHINE} == "landisk") || \
    (${MACHINE} == "i386") || (${MACHINE} == "macppc") || \
    (${MACHINE} == "sgi") || (${MACHINE} == "socppc") || \
    (${MACHINE} == "sparc64")

PROG=	pcidump

.else
NOPROG=yes
.endif

MAN=	pcidump.8

.include <bsd.prog.mk>
