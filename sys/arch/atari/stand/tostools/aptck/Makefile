#	$NetBSD: Makefile,v 1.1.1.1 1996/01/07 21:54:16 leo Exp $

PROG	= aptck.ttp

OBJS	= aptck.o biosrw.o diskio.o disklabel.o
HEADERS	= aptck.h disklabel.h

LIBS	= -lxhdi
CLEAN	= aptck.c

include ../Makefile.inc

aptck.c: aptck.in biosrw.s diskio.c disklabel.c
	${AWK} -f ./setversion $^ > aptck.c

${PROG}: ${OBJS} ${LDADD}
	${CC} ${LDFLAGS} -o $@ ${OBJS} ${LDADD} ${LIBS}
