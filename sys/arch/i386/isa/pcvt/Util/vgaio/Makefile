#	$OpenBSD: Makefile,v 1.3 1999/01/13 07:26:07 niklas Exp $

PROG=	vgaio
MAN8=	vgaio.${MAN8EXT}

SRCS=	vgaio.y lex.l
YACC=	yacc

#YFLAGS+= -yd	# Bison
#YFLAGS+= -v	# verbose

LFLAGS+= -I
LDADD=	-lm -ly -ll
DPADD=	${LIBM} ${LIBY} ${LIBL}

CFLAGS+= -I ${.CURDIR}/obj -I ${.CURDIR} -g

CLEANFILES+= y.tab.h
CLEANFILES+= y.output	# comment file from bison

.include <bsd.prog.mk>
