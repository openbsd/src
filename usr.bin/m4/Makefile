#	$OpenBSD: Makefile,v 1.2 1996/06/26 05:36:10 deraadt Exp $


# -DEXTENDED 
# 	if you want the paste & spaste macros.

PROG=	m4
CFLAGS+=-DEXTENDED
SRCS=	eval.c expr.c look.c main.c misc.c
MAN=	m4.1

.include <bsd.prog.mk>
