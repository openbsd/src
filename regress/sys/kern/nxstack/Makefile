#	$OpenBSD: Makefile,v 1.2 2002/07/26 01:32:07 deraadt Exp $

.if ${MACHINE} == "sparc64" || ${MACHINE} == "sparc" || \
    ${MACHINE} == "alpha" || ${MACHINE} == "macppc" || \
    ${MACHINE} == "mvmeppc" || ${MACHINE} == "i386"

PROG=	nxstack
LDSTATIC= ${STATIC}

.else
REGRESSTARGETS=		# this architecture has no test for this
.endif

.include <bsd.regress.mk>
