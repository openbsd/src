#	$OpenBSD: Makefile,v 1.1 2003/05/15 04:23:49 deraadt Exp $

NOMAN=
PROG=sprintf_test
CPPFLAGS+=-I${.CURDIR}/../../../../lib/libc

run-regress-atexit_test: ${PROG}
	./${PROG}

.include <bsd.regress.mk>
