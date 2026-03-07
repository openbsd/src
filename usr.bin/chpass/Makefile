#	$OpenBSD: Makefile,v 1.17 2026/03/07 18:20:25 deraadt Exp $

.include <bsd.own.mk>

PROG=	chpass
SRCS=	chpass.c edit.c field.c table.c util.c getpwent.c
BINOWN=	root
BINMODE=4555
LINKS=	${BINDIR}/chpass ${BINDIR}/chfn ${BINDIR}/chpass ${BINDIR}/chsh
CFLAGS+=-DFORCE_DBOPEN
DPADD+= ${LIBUTIL}
LDADD+= -lutil

.include <bsd.prog.mk>
