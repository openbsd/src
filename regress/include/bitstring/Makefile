#	$OpenBSD: Makefile,v 1.4 2002/09/02 20:01:43 avsm Exp $
#	$NetBSD: Makefile,v 1.4 1995/04/20 22:37:50 cgd Exp $

PROG=	./bitstring_test

REGRESS_TARGETS=test-8 test-27 test-32 test-49 test-64 test-67

test-8: ${PROG}
	${PROG} 8 | diff - ${.CURDIR}/good/8

test-27: ${PROG}
	${PROG} 27 | diff - ${.CURDIR}/good/27

test-32: ${PROG}
	${PROG} 32 | diff - ${.CURDIR}/good/32

test-49: ${PROG}
	${PROG} 49 | diff - ${.CURDIR}/good/49

test-64: ${PROG}
	${PROG} 64 | diff - ${.CURDIR}/good/64

test-67: ${PROG}
	${PROG} 67 | diff - ${.CURDIR}/good/67


.include <bsd.regress.mk>
