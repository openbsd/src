#	$OpenBSD: Makefile,v 1.5 1997/03/26 00:33:39 deraadt Exp $
#	$NetBSD: Makefile,v 1.24 1995/03/21 09:04:30 cgd Exp $

PROG=	ed
CFLAGS+=-DBACKWARDS -DDES
SRCS=	 buf.c cbc.c glbl.c io.c main.c re.c sub.c undo.c

#LINKS=  ${BINDIR}/ed ${BINDIR}/red
#MLINKS= ed.1 red.1

.include <bsd.prog.mk>
