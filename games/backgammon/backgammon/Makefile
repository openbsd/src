#	$OpenBSD: Makefile,v 1.6 1998/04/25 18:09:26 millert Exp $
#	@(#)Makefile	8.1 (Berkeley) 5/31/93

PROG=	backgammon
CFLAGS+=-I${.CURDIR}/../common_source
SRCS=	allow.c board.c check.c extra.c fancy.c init.c main.c move.c \
	odds.c one.c save.c subs.c table.c text.c
MAN=	backgammon.6
DPADD=	${LIBTERMLIB
LDADD=	-ltermlib
.PATH:	${.CURDIR}/../common_source
HIDEGAME=hidegame

.include "../../Makefile.inc"
.include <bsd.prog.mk>
