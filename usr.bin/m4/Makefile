#	$OpenBSD: Makefile,v 1.14 2017/06/17 01:55:16 bcallah Exp $

# -DEXTENDED 
# 	if you want the paste & spaste macros.

PROG=	m4
CFLAGS+=-DEXTENDED -I.
CDIAGFLAGS=-W -Wall -Wstrict-prototypes -pedantic \
	-Wno-unused -Wno-char-subscripts -Wno-sign-compare

LDADD= -lm -lutil
DPADD= ${LIBM} ${LIBUTIL}

SRCS=	eval.c expr.c look.c main.c misc.c gnum4.c trace.c tokenizer.l parser.y
MAN=	m4.1

parser.c parser.h: parser.y
	${YACC} -d ${.ALLSRC} && mv y.tab.c parser.c && mv y.tab.h parser.h

tokenizer.o: parser.h

CLEANFILES+=parser.c parser.h tokenizer.o

.include <bsd.prog.mk>
