#	$OpenBSD: Makefile,v 1.5 2003/05/03 00:08:58 mickey Exp $

.if ${MACHINE} == "sparc64" || ${MACHINE} == "sparc" || \
    ${MACHINE} == "alpha" || ${MACHINE} == "macppc" || \
    ${MACHINE} == "mvmeppc" || ${MACHINE} == "i386" || \
    ${MACHINE} == "hppa"

PROG=	noexec
CFLAGS+=-Wall -Werror
LDSTATIC= ${STATIC}
REGRESS_TARGETS=	nxtext-mmap	nxtext-mprotect \
		nxdata	nxdata-mmap	nxdata-mprotect \
		nxbss	nxbss-mmap	nxbss-mprotect \
		nxheap	nxheap-mmap	nxheap-mprotect \
		nxmmap			nxmmap-mprotect \
		nxstack	nxstack-mmap	nxstack-mprotect
.PHONY: ${REGRESS_TARGETS}

nxtext-mmap: ${PROG}
	@./${PROG} -T -m

nxtext-mprotect: ${PROG}
	@./${PROG} -T -p

nxdata: ${PROG}
	@./${PROG} -D

nxdata-mprotect: ${PROG}
	@./${PROG} -D -p

nxdata-mmap: ${PROG}
	@./${PROG} -D -m

nxbss: ${PROG}
	@./${PROG} -B

nxbss-mmap: ${PROG}
	@./${PROG} -B -m

nxbss-mprotect: ${PROG}
	@./${PROG} -B -p

nxheap: ${PROG}
	@./${PROG} -H

nxheap-mmap: ${PROG}
	@./${PROG} -H -m

nxheap-mprotect: ${PROG}
	@./${PROG} -H -p

nxstack: ${PROG}
	@./${PROG} -S

nxstack-mmap: ${PROG}
	@./${PROG} -S -m

nxstack-mprotect: ${PROG}
	@./${PROG} -S -p

nxmmap: ${PROG}
	@./${PROG} -m

nxmmap-mprotect: ${PROG}
	@./${PROG} -m -p

.else
REGRESS_TARGETS=		# this architecture has no test for this
.endif

.include <bsd.regress.mk>
