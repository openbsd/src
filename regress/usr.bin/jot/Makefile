#	$OpenBSD: Makefile,v 1.1 2016/07/30 13:55:54 tb Exp $

JOT?=jot
VERBOSE?=0

regress:
	m4 ${.CURDIR}/regress.m4 ${.CURDIR}/regress.sh | \
		env JOT=${JOT} SRCDIR=${.CURDIR} VERBOSE=${VERBOSE} sh

clean:
	rm -f test.*.out

.include <bsd.regress.mk>
