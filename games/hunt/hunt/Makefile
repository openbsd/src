#	$NetBSD: Makefile,v 1.5 1998/02/18 22:37:31 jtc Exp $
#	$OpenBSD: Makefile,v 1.3 1999/01/29 07:30:33 d Exp $

PROG=	hunt
SRCS=	connect.c hunt.c otto.c playit.c display.c
MAN=	hunt.6
LDADD=	-lcurses
DPADD=	${LIBTERMCAP} ${LIBCURSES}
HIDEGAME=hidegame

CPPFLAGS+=-I${.CURDIR}/../huntd

.PATH: ${.CURDIR}/../huntd

.include <bsd.prog.mk>
