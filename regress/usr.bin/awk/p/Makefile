# $OpenBSD: Makefile,v 1.1 2026/06/22 15:52:50 tim Exp $

REGRESS_TARGETS=	p.1 p.2 p.3 p.4 p.5 p.5a p.6 p.7 p.8 p.9 p.10 p.11 \
			p.12 p.13 p.14 p.15 p.16 p.17 p.18 p.19 p.20 p.21 \
			p.21a p.22 p.23 p.24 p.25 p.26 p.26a p.27 p.28 p.29 \
			p.30 p.31 p.32 p.33 p.34 p.35 p.36 p.37 p.38 p.39 \
			p.40 p.41 p.42 p.43 p.44 p.45 p.46 p.47 p.48 p.48a \
			p.48b p.49 p.50 p.51 p.52 p.table

CLEANFILES=		*.out
TESTFILE=		${.CURDIR}/../test.countries

.SUFFIXES: .awk

.awk:
	${AWK} -f $< ${TESTFILE} >$@.out
	cmp -s ${.CURDIR}/$@.ok $@.out

p.47:
	${AWK} -f ${.CURDIR}/$@.awk ${TESTFILE} >$@.out
	cmp -s ${.CURDIR}/$@.ok $@.out
	cmp -s ${.CURDIR}/$@.big.ok $@.big.out
	cmp -s ${.CURDIR}/$@.small.ok $@.small.out

p.48a:
	${AWK} -f ${.CURDIR}/$@.awk foo bar baz >$@.out
	cmp -s ${.CURDIR}/$@.ok $@.out

.include <bsd.regress.mk>
