#	$OpenBSD: Makefile,v 1.10 2014/04/14 22:12:01 tedu Exp $

PROG=	ed
CFLAGS+=-DBACKWARDS
SRCS=	 buf.c glbl.c io.c main.c re.c sub.c undo.c

#LINKS=  ${BINDIR}/ed ${BINDIR}/red
#MLINKS= ed.1 red.1

.include <bsd.prog.mk>
