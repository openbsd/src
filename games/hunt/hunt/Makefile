#	$NetBSD: Makefile,v 1.5 1998/02/18 22:37:31 jtc Exp $
#	$OpenBSD: Makefile,v 1.4 1999/03/14 02:07:30 pjanzen Exp $

PROG=	hunt
SRCS=	connect.c hunt.c otto.c playit.c display.c
MAN=	hunt.6
LDADD=	-lcurses
DPADD=	${LIBCURSES}
HIDEGAME=hidegame

CPPFLAGS+=-I${.CURDIR}/../huntd

.PATH: ${.CURDIR}/../huntd

.include "../../Makefile.inc"
.include <bsd.prog.mk>
