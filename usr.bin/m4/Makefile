#	$OpenBSD: Makefile,v 1.5 1999/11/17 15:34:13 espie Exp $

# -DEXTENDED 
# 	if you want the paste & spaste macros.

PROG=	m4
CFLAGS+=-DEXTENDED 
CFLAGS+=-W -Wall -Wstrict-prototypes \
	-Wno-unused -Wno-char-subscripts -Wno-sign-compare
SRCS=	eval.c expr.c look.c main.c misc.c gnum4.c
MAN=	m4.1

.include <bsd.prog.mk>
