# $OpenBSD: Makefile,v 1.1 2002/06/07 05:02:58 hin Exp $

AFSTOPDIR	= ../..
.include <../../Makefile.inc>

PROG	= pts
SRCS	= pts.c
LDADD	+= ${LIBARLA} ${LIBROKEN} -lkafs -lkrb -lreadline -ldes -lcurses
DPADD	+= ${LIBARLA} ${LIBROKEN} ${LIBKAFS}

.PATH: ${AFSSRCDIR}/appl/pts

SRCS	+= arlalib.c fs_lib.c
CFLAGS	+= -I${AFSSRC}/appl/lib
.PATH: ${AFSSRC}/appl/lib
.PATH: ${AFSSRC}/appl/pts
.PATH: ${AFSSRC}/arlad

.include <bsd.prog.mk>
