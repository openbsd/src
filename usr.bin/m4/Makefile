#	$OpenBSD: Makefile,v 1.16 2017/07/09 14:04:50 espie Exp $

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

.include <bsd.prog.mk>
