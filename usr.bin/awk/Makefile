#	$OpenBSD: Makefile,v 1.17 2020/06/10 21:01:50 millert Exp $

PROG=	awk
SRCS=	ytab.c lex.c b.c main.c parse.c proctab.c tran.c lib.c run.c
LDADD=	-lm
DPADD=	${LIBM}
CLEANFILES+=proctab.c maketab ytab.c ytab.h
CFLAGS+=-I. -I${.CURDIR} -DHAS_ISBLANK -DNDEBUG
HOSTCFLAGS+=-I. -I${.CURDIR} -DHAS_ISBLANK -DNDEBUG

ytab.c ytab.h: awkgram.y
	${YACC} -o ytab.c -d ${.CURDIR}/awkgram.y

BUILDFIRST = ytab.h

proctab.c: maketab
	./maketab ytab.h >proctab.c

maketab: ytab.h maketab.c
	${HOSTCC} ${HOSTCFLAGS} ${.CURDIR}/maketab.c -o $@

.include <bsd.prog.mk>
