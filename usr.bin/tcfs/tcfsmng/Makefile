#	$OpenBSD: Makefile,v 1.3 2000/06/20 08:36:17 fgsch Exp $

.PATH:		${.CURDIR}/..

PROG=	tcfsmng
BINOWN=	root

BINDIR=	/usr/bin
NOMAN=
#MAN=	tcfsmng.1

SRCS=	tcfsmng.c tcfsadduser.c tcfsrmuser.c tcfsaddgroup.c tcfsrmgroup.c

.include <bsd.prog.mk>

LDADD+=	-lutil
DPADD+= ${LIBUTIL}
