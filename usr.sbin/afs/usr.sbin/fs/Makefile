# $OpenBSD: Makefile,v 1.1 2002/06/07 05:02:58 hin Exp $

AFSTOPDIR	= ../..
.include <../../Makefile.inc>

PROG	= fs
BINDIR	= /usr/sbin
LDADD	+= ${LIBARLA} ${LIBROKEN} -lkafs -lkrb -lreadline -ldes -lcurses
DPADD	+= ${LIBARLA} ${LIBROKEN} ${LIBKAFS}

CFLAGS	+= -I${AFSSRC}/appl/lib

SRCS	= fs.c
.PATH: ${AFSSRC}/appl/fs

SRCS	+= arladebu.c fs_lib.c

.PATH: ${AFSSRC}/appl/lib
.PATH: ${AFSSRC}/arlad

.include <bsd.prog.mk>
