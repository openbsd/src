#	$OpenBSD: Makefile,v 1.5 2007/05/26 03:00:03 mjc Exp $

PROG=	cdio
DPADD=	${LIBUTIL} ${LIBEDIT} ${LIBTERMCAP}
LDADD=	-lutil -ledit -ltermcap
SRCS=	cdio.c cddb.c mmc.c rip.c
CDIAGFLAGS=-Wall -W -Wmissing-prototypes -pedantic

.include <bsd.prog.mk>
