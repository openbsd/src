PROG=	awk
SRCS=	awkgram.c awklex.c b.c main.c parse.c proctab.c tran.c lib.c run.c
LDADD=	-ll -lm
DPADD=	${LIBL} ${LIBM}
CLEANFILES+=awkgram.c awkgram.h awklex.c awklex.h proctab.c maketab prevawkgram.h
CFLAGS+=-I. -I${.CURDIR}

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
