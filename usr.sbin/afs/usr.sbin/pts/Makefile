# $OpenBSD: Makefile,v 1.3 2003/08/05 11:11:42 hin Exp $

AFSTOPDIR	= ../..
.include <../../Makefile.inc>

.PATH:	$(ROKENSRC)

PROG	= pts
SRCS	= pts.c \
	  estrdup.c
LDADD	+= ${LIBARLA} -lkrb5 -lcrypto -lasn1 -lreadline -ldes -lcurses
DPADD	+= ${LIBARLA}

.PATH: ${AFSSRCDIR}/appl/pts

SRCS	+= arlalib.c fs_lib.c
CFLAGS	+= -I${AFSSRC}/appl/lib
.PATH: ${AFSSRC}/appl/lib
.PATH: ${AFSSRC}/appl/pts
.PATH: ${AFSSRC}/arlad

.include <bsd.prog.mk>
