#	$OpenBSD: Makefile,v 1.1 2002/04/30 23:22:58 millert Exp $

PROG=	sigfpe

# XXX - don't want the compiler to optimize away our tests
CFLAGS=   -O0 ${PIPE} ${DEBUG}

REGRESSTARGETS=	divzero oflow

divzero:
	./${PROG} -z

oflow:
	./${PROG} -o

.include <bsd.regress.mk>
