#	$NetBSD: Makefile,v 1.24 1995/03/21 09:04:30 cgd Exp $

PROG=	ed
CFLAGS+=-DBACKWARDS  #-DDES
SRCS=	 buf.c cbc.c glbl.c io.c main.c re.c sub.c undo.c

LDADD+= -lcrypt
DPADD+= ${LIBCRYPT}

#LINKS=  ${BINDIR}/ed ${BINDIR}/red
#MLINKS= ed.1 red.1

.include <bsd.prog.mk>
