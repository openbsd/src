#	$NetBSD: Makefile,v 1.5 1995/03/21 15:06:07 cgd Exp $
#	@(#)Makefile	8.1 (Berkeley) 5/31/93

PROG=	teachgammon
CFLAGS+=-I${.CURDIR}/../common_source
SRCS=	data.c teach.c ttext1.c ttext2.c tutor.c
DPADD=	${.CURDIR}/../common_source/libcommon.a ${LIBTERMCAP}
LDADD=	-L${.CURDIR}/../common_source -lcommon -ltermcap
HIDEGAME=hidegame
NOMAN=	noman

.PATH:	${.CURDIR}/../common_source

.include <bsd.prog.mk>
