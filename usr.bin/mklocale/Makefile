#	$OpenBSD: Makefile,v 1.2 2009/08/08 19:37:14 sobrado Exp $
#	$NetBSD: Makefile,v 1.11 2003/08/01 17:04:04 lukem Exp $

.include <bsd.own.mk>

PROG=		mklocale
SRCS=		yacc.y lex.l
CPPFLAGS+=	-I. -I${.CURDIR} -I${.CURDIR}/../../lib/libc -I${.CURDIR}/../../lib/libc/include
YHEADER=	1

yacc.c yacc.h: yacc.y
	${YACC} -d ${.ALLSRC} && mv y.tab.c yacc.c && mv y.tab.h yacc.h


CLEANFILES+= yacc.c yacc.h
.include <bsd.prog.mk>
