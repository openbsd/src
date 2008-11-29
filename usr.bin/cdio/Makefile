#	$OpenBSD: Makefile,v 1.6 2008/11/29 08:57:10 jakemsr Exp $

PROG=	cdio
DPADD=	${LIBUTIL} ${LIBEDIT} ${LIBTERMCAP}
LDADD=	-lutil -ledit -ltermcap -lsndio
SRCS=	cdio.c cddb.c mmc.c rip.c
CDIAGFLAGS=-Wall -W -Wmissing-prototypes -pedantic

.include <bsd.prog.mk>
