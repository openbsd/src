#	$NetBSD: Makefile,v 1.1 1996/01/07 22:06:22 leo Exp $

PROG	= rawwrite.ttp

OBJS	= rawwrite.o
HEADERS	= 

include ../Makefile.inc

${PROG}: ${OBJS} ${LDADD}
	${CC} ${LDFLAGS} -o $@ ${OBJS} ${LDADD} ${LIBS}
