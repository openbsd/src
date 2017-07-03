#	$OpenBSD: Makefile,v 1.3 2017/07/03 15:17:11 espie Exp $
#	$NetBSD: Makefile,v 1.11 2003/08/01 17:04:04 lukem Exp $

.include <bsd.own.mk>

PROG=		mklocale
SRCS=		yacc.y lex.l
CPPFLAGS+=	-I. -I${.CURDIR} -I${.CURDIR}/../../lib/libc -I${.CURDIR}/../../lib/libc/include
YHEADER=	1

yacc.c yacc.h: yacc.y
	${YACC} -d ${.ALLSRC} && mv y.tab.c yacc.c && mv y.tab.h yacc.h

lex.o: yacc.h

CLEANFILES+= yacc.c yacc.h
.include <bsd.prog.mk>
