#	$OpenBSD: Makefile,v 1.1 2002/08/31 22:56:01 mickey Exp $

.if ${MACHINE} == "sparc64" || ${MACHINE} == "sparc" || \
    ${MACHINE} == "alpha" || ${MACHINE} == "macppc" || \
    ${MACHINE} == "mvmeppc" || ${MACHINE} == "i386" || \
    ${MACHINE} == "hppa"

PROG=	noexec
CFLAGS+=-Wall -Werror
LDSTATIC= ${STATIC}
REGRESSTARGETS=		nxtext-mprotect \
		nxdata	nxdata-mprotect \
		nxbss	nxbss-mprotect \
		nxheap	nxheap-mprotect \
		nxstack	nxstack-mprotect

nxtext-mprotect: ${PROG}
	#@./${PROG} -T -p

nxdata: ${PROG}
	@./${PROG} -D

nxdata-mprotect: ${PROG}
	@./${PROG} -D -p

nxbss: ${PROG}
	@./${PROG} -B

nxbss-mprotect: ${PROG}
	@./${PROG} -B -p

nxheap: ${PROG}
	@./${PROG} -H

nxheap-mprotect: ${PROG}
	@./${PROG} -H -p

nxstack: ${PROG}
	@./${PROG} -S

nxstack-mprotect: ${PROG}
	@./${PROG} -S -p

.else
REGRESSTARGETS=		# this architecture has no test for this
.endif

.include <bsd.regress.mk>
