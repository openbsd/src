#	$OpenBSD: Makefile,v 1.4 1997/01/24 23:11:29 millert Exp $

PROG=	awk
LINKS=	${BINDIR}/awk ${BINDIR}/nawk
SRCS=	awkgram.c lexyy.c b.c main.c parse.c proctab.c tran.c lib.c run.c
LDADD=	-ll -lm
DPADD=	${LIBL} ${LIBM}
CLEANFILES+=awkgram.c awkgram.h proctab.c maketab prevawkgram.h
CFLAGS+=-I. -I${.CURDIR}
MLINKS=	awk.1 nawk.1

awkgram.c awkgram.h prevawkgram.h: awkgram.y
	${YACC} -d ${.IMPSRC}
	mv y.tab.c awkgram.c
	mv y.tab.h awkgram.h
	cmp -s awkgram.h prevawkgram.h || cp awkgram.h prevawkgram.h

proctab.c: maketab
	./maketab >proctab.c

maketab: prevawkgram.h
	${HOSTCC} ${CFLAGS} ${.CURDIR}/maketab.c -o $@

.include <bsd.prog.mk>
