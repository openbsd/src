#	$NetBSD: Makefile,v 1.5 1998/02/18 22:37:31 jtc Exp $
#	$OpenBSD: Makefile,v 1.2 1999/01/21 05:47:38 d Exp $

PROG=	hunt
SRCS=	connect.c hunt.c otto.c playit.c pathname.c
MAN=	hunt.6
LDADD=	-locurses
DPADD=	${LIBTERMCAP} ${LIBOLDCURSES}
HIDEGAME=hidegame

CPPFLAGS+=-I${.CURDIR}/../huntd -D_USE_OLD_CURSES_

.PATH: ${.CURDIR}/../huntd

.include <bsd.prog.mk>
