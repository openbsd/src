#	$NetBSD: Makefile,v 1.5 1995/03/21 15:06:07 cgd Exp $
#	@(#)Makefile	8.1 (Berkeley) 5/31/93

PROG=	teachgammon
CFLAGS+=-I${.CURDIR}/../common_source
SRCS=	allow.c board.c check.c data.c fancy.c init.c odds.c one.c save.c \
	subs.c table.c teach.c ttext1.c ttext2.c tutor.c
DPADD=	${LIBTERMLIB}
LDADD=	-ltermlib
HIDEGAME=hidegame
NOMAN=	noman

.PATH:	${.CURDIR}/../common_source

.include "../../Makefile.inc"
.include <bsd.prog.mk>
