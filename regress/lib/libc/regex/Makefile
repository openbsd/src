#	$OpenBSD: Makefile,v 1.4 2002/01/01 23:00:51 art Exp $
#	$NetBSD: Makefile,v 1.2 1995/02/16 19:38:45 cgd Exp $

PROG=	re
SRCS=	main.c split.c debug.c

CFLAGS+= -I${.CURDIR}/../../../../lib/libc/regex

TESTS=	${.CURDIR}/tests

REGRESSTARGETS=do-reg do-reg-long do-reg-backref

do-reg: ${PROG}
	./re < ${TESTS}
do-reg-long: ${PROG}
	./re -el < ${TESTS}
do-reg-backref: ${PROG}
	./re -er < ${TESTS}

.include <bsd.regress.mk>
