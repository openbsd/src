#	$OpenBSD: Makefile,v 1.2 2002/04/18 20:18:31 espie Exp $

PROG=	cdio
DPADD=	${LIBUTIL}
LDADD=	-lutil
SRCS=	cdio.c cddb.c
CDIAGFLAGS=-Wall -W -Wmissing-prototypes -pedantic

.include <bsd.prog.mk>
