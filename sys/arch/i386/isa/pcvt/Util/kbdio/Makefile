#	$OpenBSD: Makefile,v 1.3 1999/01/13 07:26:04 niklas Exp $

PROG=	kbdio
SRCS=	kbdio.y lex.l

#YACC=	bison
#YFLAGS+= -yd	# Bison only

YFLAGS+= -v	# verbose
LFLAGS+= -I

LDADD=	-lm -ly -ll
DPADD=	${LIBM} ${LIBY} ${LIBL}

NOMAN=
NOSHARED=	no shared linkage

CFLAGS+= -I ${.CURDIR}/obj -I ${.CURDIR} #-g

CLEANFILES+= y.tab.h
CLEANFILES+= y.output	# comment file from bison

.include <bsd.prog.mk>

lex.o: y.tab.h lex.l
