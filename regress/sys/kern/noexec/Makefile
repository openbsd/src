#	$OpenBSD: Makefile,v 1.2 2002/09/02 20:01:44 avsm Exp $

.if ${MACHINE} == "sparc64" || ${MACHINE} == "sparc" || \
    ${MACHINE} == "alpha" || ${MACHINE} == "macppc" || \
    ${MACHINE} == "mvmeppc" || ${MACHINE} == "i386" || \
    ${MACHINE} == "hppa"

PROG=	noexec
CFLAGS+=-Wall -Werror
LDSTATIC= ${STATIC}
REGRESS_TARGETS=nxtext-mprotect \
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
REGRESS_TARGETS=		# this architecture has no test for this
.endif

.include <bsd.regress.mk>
