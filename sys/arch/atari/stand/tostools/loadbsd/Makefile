#	$NetBSD: Makefile,v 1.1 1996/01/07 22:06:13 leo Exp $

PROG	= loadbsd.ttp

OBJS	= loadbsd.o
HEADERS	= loader.h ../libtos/kparamb.h

include ../Makefile.inc

${PROG}: ${OBJS} ${LDADD}
	${CC} ${LDFLAGS} -o $@ ${OBJS} ${LDADD} ${LIBS}
