#	$OpenBSD: Makefile,v 1.1 2002/08/27 16:37:42 matthieu Exp $

.if ${MACHINE} == "sparc64" || ${MACHINE} == "sparc" || \
    ${MACHINE} == "alpha" || ${MACHINE} == "macppc" || \
    ${MACHINE} == "mvmeppc" || ${MACHINE} == "i386" || \
    ${MACHINE} == "hppa"

PROG=	nxheap-mprotect
LDSTATIC= ${STATIC}

.else
REGRESSTARGETS=		# this architecture has no test for this
.endif

.include <bsd.regress.mk>
