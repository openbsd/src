#	$OpenBSD: Makefile,v 1.1 2002/08/31 20:29:47 mickey Exp $

.if ${MACHINE} == "sparc64" || ${MACHINE} == "sparc" || \
    ${MACHINE} == "alpha" || ${MACHINE} == "macppc" || \
    ${MACHINE} == "mvmeppc" || ${MACHINE} == "i386" || \
    ${MACHINE} == "hppa"

PROG=	nxheap
LDSTATIC= ${STATIC}

.else
REGRESSTARGETS=		# this architecture has no test for this
.endif

.include <bsd.regress.mk>
