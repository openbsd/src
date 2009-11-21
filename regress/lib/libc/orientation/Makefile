#	$OpenBSD: Makefile,v 1.1 2009/11/21 09:56:10 guenther Exp $

NOMAN=
PROG=orientation_test
CPPFLAGS+=-I${.CURDIR}/../../../../lib/libc

run-regress-atexit_test: ${PROG}
	./${PROG}

.include <bsd.regress.mk>
