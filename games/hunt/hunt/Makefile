#	$NetBSD: Makefile,v 1.5 1998/02/18 22:37:31 jtc Exp $
#	$OpenBSD: Makefile,v 1.6 1999/12/12 14:53:02 d Exp $

PROG=	hunt
SRCS=	connect.c hunt.c otto.c playit.c display.c list.c
MAN=	hunt.6
LDADD=	-lcurses
DPADD=	${LIBCURSES}
HIDEGAME=hidegame

CPPFLAGS+=-I${.CURDIR}/../huntd

.include "../../Makefile.inc"
.include <bsd.prog.mk>
