#	$OpenBSD: Makefile,v 1.5 2001/08/17 10:05:18 espie Exp $
#	$NetBSD: Makefile,v 1.2 1999/02/13 02:54:33 lukem Exp $

NOMAN=
NOPROG=

FIBOMAX=25
M4=m4

CLEANFILES+= ff_after_dnl.m4 fibo.out

regress: test-ff_after_dnl test-m4wrap test-fibo

test-ff_after_dnl: ff_after_dnl.m4
	${M4} ff_after_dnl.m4 | diff - ${.CURDIR}/ff_after_dnl.out

ff_after_dnl.m4: ff_after_dnl.m4.uu
	uudecode ${.CURDIR}/ff_after_dnl.m4.uu

test-m4wrap:
	${M4} ${.CURDIR}/m4wrap.m4 | diff - ${.CURDIR}/m4wrap.out

fibo.out:
	perl ${.CURDIR}/fibo.pl ${FIBOMAX} >$@

test-fibo: fibo.out
	${M4} -DN=${FIBOMAX} ${.CURDIR}/fibo.m4| diff - fibo.out

.PHONY:	test-ff_after_dnl test-m4wrap test-fibo

.include <bsd.prog.mk>
