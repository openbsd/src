#	$NetBSD: Makefile,v 1.5 1998/02/18 22:37:31 jtc Exp $
#	$OpenBSD: Makefile,v 1.5 1999/08/30 23:32:10 d Exp $

PROG=	hunt
SRCS=	connect.c hunt.c otto.c playit.c display.c
MAN=	hunt.6
LDADD=	-lcurses
DPADD=	${LIBCURSES}
HIDEGAME=hidegame

CPPFLAGS+=-I${.CURDIR}/../huntd

.include "../../Makefile.inc"
.include <bsd.prog.mk>
