#	$OpenBSD: Makefile,v 1.3 1998/03/19 11:13:03 pjanzen Exp $
#	@(#)Makefile	8.1 (Berkeley) 5/31/93

PROG=	backgammon
CFLAGS+=-I${.CURDIR}/../common_source
SRCS=	extra.c main.c move.c text.c
MAN=	backgammon.6
DPADD=	${.CURDIR}/../common_source/libcommon.a ${LIBTERMCAP}
LDADD=	-L${.CURDIR}/../common_source -lcommon -ltermcap
.PATH:	${.CURDIR}/../common_source
HIDEGAME=hidegame

.include "../../Makefile.inc"
.include <bsd.prog.mk>
