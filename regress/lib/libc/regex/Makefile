#	$OpenBSD: Makefile,v 1.3 2001/02/04 14:44:00 ericj Exp $
#	$NetBSD: Makefile,v 1.2 1995/02/16 19:38:45 cgd Exp $

PROG=	re
SRCS=	main.c split.c debug.c
NOMAN=

CFLAGS+= -I${.CURDIR}/../../../../lib/libc/regex

TESTS=	${.CURDIR}/tests

regress: ${PROG}
	./re < ${TESTS}
	./re -el < ${TESTS}
	./re -er < ${TESTS}

.include <bsd.prog.mk>
