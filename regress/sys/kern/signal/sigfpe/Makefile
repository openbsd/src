#	$OpenBSD: Makefile,v 1.2 2002/08/23 15:31:00 art Exp $

PROG=	sigfpe

# XXX - don't want the compiler to optimize away our tests
CFLAGS=   -O0 ${PIPE} ${DEBUG}

REGRESSTARGETS=	divzero oflow

divzero: ${PROG}
	./${PROG} -z

oflow: ${PROG}
	./${PROG} -o

.include <bsd.regress.mk>
