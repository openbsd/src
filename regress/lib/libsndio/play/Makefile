#	$OpenBSD: Makefile,v 1.2 2010/11/06 20:25:42 ratchov Exp $
PROG= play
LDADD= -lsndio
SRCS = play.c tools.c
REGRESS_SKIP=
.PATH: ${.CURDIR}/..
.include <bsd.regress.mk>
