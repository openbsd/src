#	$OpenBSD: Makefile,v 1.2 2000/07/01 00:49:07 espie Exp $
#	$NetBSD: Makefile,v 1.2 1999/02/13 02:54:33 lukem Exp $

NOMAN=
NOPROG=

CLEANFILES+= ff_after_dnl.m4

regress: test-ff_after_dnl test-m4wrap test-fibo

test-ff_after_dnl: ff_after_dnl.m4
	m4 ff_after_dnl.m4 | diff - ${.CURDIR}/ff_after_dnl.out

ff_after_dnl.m4: ff_after_dnl.m4.uu
	uudecode ${.CURDIR}/ff_after_dnl.m4.uu

test-m4wrap:
	m4 ${.CURDIR}/m4wrap.m4 | diff - ${.CURDIR}/m4wrap.out

test-fibo:
	m4 ${.CURDIR}/fibo.m4 | diff - ${.CURDIR}/fibo.out

.PHONY:	test-ff_after_dnl test-m4wrap test-fibo

.include <bsd.prog.mk>
