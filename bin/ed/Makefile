#	$OpenBSD: Makefile,v 1.9 2010/01/04 17:50:37 deraadt Exp $

PROG=	ed
CFLAGS+=-DBACKWARDS -DDES
SRCS=	 buf.c cbc.c glbl.c io.c main.c re.c sub.c undo.c

#LINKS=  ${BINDIR}/ed ${BINDIR}/red
#MLINKS= ed.1 red.1

.include <bsd.prog.mk>
