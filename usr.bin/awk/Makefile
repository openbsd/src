#	$OpenBSD: Makefile,v 1.2 1996/07/06 05:40:02 etheisen Exp $

PROG=	awk
LINKS=	${BINDIR}/awk ${BINDIR}/nawk
SRCS=	awkgram.c awklex.c b.c main.c parse.c proctab.c tran.c lib.c run.c
LDADD=	-ll -lm
DPADD=	${LIBL} ${LIBM}
CLEANFILES+=awkgram.c awkgram.h awklex.c awklex.h proctab.c maketab prevawkgram.h
CFLAGS+=-I. -I${.CURDIR}
MLINKS=	awk.1 nawk.1

awkgram.c awkgram.h prevawkgram.h: awkgram.y
	${YACC} -d ${.IMPSRC}
	mv y.tab.c awkgram.c
	mv y.tab.h awkgram.h
	cmp -s awkgram.h prevawkgram.h || cp awkgram.h prevawkgram.h

awklex.c: awklex.l
	lex ${.IMPSRC}
	mv lex.yy.c awklex.c

proctab.c: maketab
	./maketab >proctab.c

maketab.o: prevawkgram.h

.include <bsd.prog.mk>
