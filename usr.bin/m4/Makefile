#	$OpenBSD: Makefile,v 1.7 2001/07/18 13:28:47 espie Exp $

# -DEXTENDED 
# 	if you want the paste & spaste macros.

PROG=	m4
CFLAGS+=-DEXTENDED
CDIAGFLAGS=-W -Wall -Wstrict-prototypes \
	-Wno-unused -Wno-char-subscripts -Wno-sign-compare
# No optimization for m88k
.if (${MACHINE_ARCH} == "m88k")
CFLAGS+=-O0
.endif

SRCS=	eval.c expr.c look.c main.c misc.c gnum4.c
MAN=	m4.1

.include <bsd.prog.mk>
