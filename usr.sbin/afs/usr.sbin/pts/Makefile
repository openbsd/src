# $OpenBSD: Makefile,v 1.5 2010/10/13 08:14:22 jsg Exp $

AFSTOPDIR	= ../..
.include <../../Makefile.inc>

.PATH:	$(ROKENSRC)

PROG	= pts
SRCS	= pts.c \
	  estrdup.c
LDADD	+= ${LIBARLA} -lkrb5 -lcrypto -lasn1 -ledit -lcurses
DPADD	+= ${LIBARLA}

.PATH: ${AFSSRCDIR}/appl/pts

SRCS	+= arlalib.c fs_lib.c
CFLAGS	+= -I${AFSSRC}/appl/lib
.PATH: ${AFSSRC}/appl/lib
.PATH: ${AFSSRC}/appl/pts
.PATH: ${AFSSRC}/arlad

.include <bsd.prog.mk>
