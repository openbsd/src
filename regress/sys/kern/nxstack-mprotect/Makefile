#	$OpenBSD: Makefile,v 1.2 2002/07/27 08:01:31 deraadt Exp $

.if ${MACHINE} == "sparc64" || ${MACHINE} == "sparc" || \
    ${MACHINE} == "alpha" || ${MACHINE} == "macppc" || \
    ${MACHINE} == "mvmeppc" || ${MACHINE} == "i386" || \
    ${MACHINE} == "hppa"

PROG=	nxstack-mprotect
LDSTATIC= ${STATIC}

.else
REGRESSTARGETS=		# this architecture has no test for this
.endif

.include <bsd.regress.mk>
