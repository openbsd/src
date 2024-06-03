#	$OpenBSD: Makefile,v 1.19 2024/06/03 00:55:05 millert Exp $

WARNINGS=Yes
PROG=	awk
SRCS=	awkgram.tab.c lex.c b.c main.c parse.c proctab.c tran.c lib.c run.c
LDADD=	-lm
DPADD=	${LIBM}
CLEANFILES+=proctab.c maketab awkgram.tab.c awkgram.tab.h
CFLAGS+=-I. -I${.CURDIR} -DHAS_ISBLANK -DNDEBUG
HOSTCFLAGS+=-I. -I${.CURDIR} -DHAS_ISBLANK -DNDEBUG

awkgram.tab.c awkgram.tab.h: awkgram.y
	${YACC} -o awkgram.tab.c -d ${.CURDIR}/awkgram.y

BUILDFIRST = awkgram.tab.h

proctab.c: maketab
	./maketab awkgram.tab.h >proctab.c

maketab: awkgram.tab.h maketab.c
	${HOSTCC} ${HOSTCFLAGS} ${.CURDIR}/maketab.c -o $@

.include <bsd.prog.mk>
