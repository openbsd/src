#	$NetBSD: Makefile,v 1.2 1996/01/16 15:14:53 leo Exp $

PROG	= aptck.ttp

OBJS	= aptck.o biosrw.o diskio.o disklbl.o
HEADERS	= aptck.h ahdilbl.h disklbl.h

LIBS	= -lxhdi
CLEAN	= aptck.c

include ../Makefile.inc

aptck.c: aptck.in biosrw.s diskio.c disklbl.c
	${AWK} -f ./setrev.awk $^ > aptck.c

${PROG}: ${OBJS} ${LDADD}
	${CC} ${LDFLAGS} -o $@ ${OBJS} ${LDADD} ${LIBS}
