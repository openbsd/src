#	$OpenBSD: Makefile,v 1.2 2002/07/26 01:32:07 deraadt Exp $

.if ${MACHINE} == "sparc64" || ${MACHINE} == "sparc" || \
    ${MACHINE} == "alpha"

PROG=	nxbss
LDSTATIC= ${STATIC}

.else
REGRESSTARGETS=		# this architecture has no test for this
.endif

.include <bsd.regress.mk>
