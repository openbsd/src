#	$NetBSD: Makefile,v 1.1.1.1 1996/01/07 21:50:50 leo Exp $
#
LIB	= libtos.a

OBJS	= bsdstart.o eprintf.o error.o xalloc.o xexit.o keywait.o
HEADERS	= kparamb.h libtos.h

include ../Makefile.inc

${LIB}: ${OBJS}
	@${RM} ${LIB}
	${AR} ${LIB} ${OBJS}
