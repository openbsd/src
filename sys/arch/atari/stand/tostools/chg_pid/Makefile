#	$NetBSD: Makefile,v 1.1 1996/01/07 22:06:01 leo Exp $

PROG	= chg_pid.ttp

OBJS	= chg_pid.o
HEADERS	=

include ../Makefile.inc

${PROG}: ${OBJS} ${LDADD}
	${CC} ${LDFLAGS} -o $@ ${OBJS} ${LDADD} ${LIBS}
