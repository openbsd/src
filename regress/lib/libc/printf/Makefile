#	$OpenBSD: Makefile,v 1.2 2020/07/08 01:18:04 schwarze Exp $

PROGS =			fp string

REGRESS_TARGETS =	${PROGS:S/^/run-regress-/}

.for p in ${PROGS}
run-regress-${p}: ${p}
	./${p}
.PHONY: run-regress-${p}
.endfor

.include <bsd.regress.mk>
