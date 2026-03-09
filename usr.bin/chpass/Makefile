#	$OpenBSD: Makefile,v 1.18 2026/03/09 12:56:12 deraadt Exp $

.include <bsd.own.mk>

PROG=	chpass
SRCS=	chpass.c edit.c field.c table.c util.c getpwent.c
BINOWN=	root
BINMODE=4555
LINKS=	${BINDIR}/chpass ${BINDIR}/chfn ${BINDIR}/chpass ${BINDIR}/chsh
DPADD+= ${LIBUTIL}
LDADD+= -lutil

.include <bsd.prog.mk>
