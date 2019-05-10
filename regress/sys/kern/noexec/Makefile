#	$OpenBSD: Makefile,v 1.20 2019/05/10 15:58:22 visa Exp $

PROG=		noexec
SRCS=		noexec.c testfly.S
CFLAGS=		-Wall -fno-inline
LDSTATIC=	${STATIC}

REGRESS_TARGETS=	nxtext-mmap	nxtext-mprotect \
		nxdata	nxdata-mmap	nxdata-mprotect \
		nxbss	nxbss-mmap	nxbss-mprotect \
		nxheap	nxheap-mmap	nxheap-mprotect \
		nxmmap			nxmmap-mprotect \
		nxstack	nxstack-mmap	nxstack-mprotect
.PHONY: ${REGRESS_TARGETS}

nxtext-mmap: ${PROG}
	@echo '\n====== $@ ======'
	./${PROG} -T -m

nxtext-mprotect: ${PROG}
	@echo '\n====== $@ ======'
	./${PROG} -T -p

nxdata: ${PROG}
	@echo '\n====== $@ ======'
	./${PROG} -D

nxdata-mprotect: ${PROG}
	@echo '\n====== $@ ======'
	./${PROG} -D -p

nxdata-mmap: ${PROG}
	@echo '\n====== $@ ======'
	./${PROG} -D -m

nxbss: ${PROG}
	@echo '\n====== $@ ======'
	./${PROG} -B

nxbss-mmap: ${PROG}
	@echo '\n====== $@ ======'
	./${PROG} -B -m

nxbss-mprotect: ${PROG}
	@echo '\n====== $@ ======'
	./${PROG} -B -p

nxheap: ${PROG}
	@echo '\n====== $@ ======'
	./${PROG} -H

nxheap-mmap: ${PROG}
	@echo '\n====== $@ ======'
	./${PROG} -H -m

nxheap-mprotect: ${PROG}
	@echo '\n====== $@ ======'
	./${PROG} -H -p

nxstack: ${PROG}
	@echo '\n====== $@ ======'
	./${PROG} -S

nxstack-mmap: ${PROG}
	@echo '\n====== $@ ======'
	./${PROG} -S -m

nxstack-mprotect: ${PROG}
	@echo '\n====== $@ ======'
	./${PROG} -S -p

nxmmap: ${PROG}
	@echo '\n====== $@ ======'
	./${PROG} -m

nxmmap-mprotect: ${PROG}
	@echo '\n====== $@ ======'
	./${PROG} -m -p

.if ${MACHINE} != "alpha" && \
    ${MACHINE} != "amd64" && \
    ${MACHINE} != "arm64" && \
    ${MACHINE} != "armv7" && \
    ${MACHINE} != "hppa" && \
    ${MACHINE} != "i386" && \
    ${MACHINE} != "octeon" && \
    ${MACHINE} != "sparc64"
REGRESS_TARGETS=run-regress-skiparch
run-regress-skiparch:
	# this architecture has no test for this
	@echo SKIPPED
.endif

.include <bsd.regress.mk>
