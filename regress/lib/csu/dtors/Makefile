#	$OpenBSD: Makefile,v 1.2 2002/02/18 11:09:48 art Exp $

PROG=dtors

REGRESSTARGETS=dtor-exit dtor-return

dtor-exit: dtors
	./dtors

dtor-return: dtors
	./dtors -f

.include <bsd.regress.mk>
