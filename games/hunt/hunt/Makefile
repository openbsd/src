#	$NetBSD: Makefile,v 1.5 1998/02/18 22:37:31 jtc Exp $

PROG=	hunt
SRCS=	connect.c hunt.c otto.c playit.c pathname.c
MAN=	hunt.6
LDADD=	-lcurses
DPADD=	${LIBTERMCAP}
HIDEGAME=hidegame

CPPFLAGS+=-I${.CURDIR}/../huntd

.PATH: ${.CURDIR}/../huntd

.include <bsd.prog.mk>
