#	$OpenBSD: Makefile,v 1.5 1997/08/25 16:17:08 kstailey Exp $

PROG=	awk
LINKS=	${BINDIR}/awk ${BINDIR}/nawk
SRCS=	ytab.c lex.c b.c main.c parse.c proctab.c tran.c lib.c run.c
LDADD=	-lm
DPADD=	${LIBM}
CLEANFILES+=proctab.c maketab ytab.c ytab.h
CFLAGS+=-I. -I${.CURDIR}
MLINKS=	awk.1 nawk.1

ytab.c ytab.h: awkgram.y
	${YACC} -d ${.CURDIR}/awkgram.y
	mv y.tab.c ytab.c
	mv y.tab.h ytab.h

proctab.c: maketab
	./maketab >proctab.c

maketab: ytab.h maketab.c
	${HOSTCC} ${CFLAGS} ${.CURDIR}/maketab.c -o $@

.include <bsd.prog.mk>
