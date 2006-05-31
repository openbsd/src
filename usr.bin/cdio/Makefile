#	$OpenBSD: Makefile,v 1.4 2006/05/31 01:14:41 mjc Exp $

PROG=	cdio
DPADD=	${LIBUTIL} ${LIBEDIT} ${LIBTERMCAP}
LDADD=	-lutil -ledit -ltermcap
SRCS=	cdio.c cddb.c mmc.c
CDIAGFLAGS=-Wall -W -Wmissing-prototypes -pedantic

.include <bsd.prog.mk>
