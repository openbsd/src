#	$OpenBSD: Makefile,v 1.3 2003/02/13 05:48:38 fgsch Exp $

PROG=	cdio
DPADD=	${LIBUTIL} ${LIBEDIT} ${LIBTERMCAP}
LDADD=	-lutil -ledit -ltermcap
SRCS=	cdio.c cddb.c
CDIAGFLAGS=-Wall -W -Wmissing-prototypes -pedantic

.include <bsd.prog.mk>
