#	from: @(#)Makefile	5.12 (Berkeley) 7/1/90
#	$Id: Makefile,v 1.1.1.1 1995/10/18 08:47:36 deraadt Exp $

PROG=	htable
SRCS=	htable.c parse.c scan.c
MAN=	htable.8
CFLAGS+=-I. -I${.CURDIR}
LDADD+=	-ll
DPADD+=	${LIBL}
CLEANFILES+=parse.c scan.c y.tab.h

.include <bsd.prog.mk>

${OBJS}: scan.c parse.c
