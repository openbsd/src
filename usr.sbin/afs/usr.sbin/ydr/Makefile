#	$OpenBSD: Makefile,v 1.3 2004/01/05 02:55:28 espie Exp $

AFSTOPDIR=	../..
.include <../../Makefile.inc>

.PATH:	$(ROKENSRC)

test:
	echo $(ROKENSRC)

SRCS		+= main.c output.c symbol.c types.c lex.c parse.c list.c \
		   hash.c arlamath.c emalloc.c estrdup.c strupr.c
CLEANFILES	+= lex.c parse.c parse.h y.tab.h
CFLAGS		+= -DCPP='"/usr/bin/cpp"'
MAN		=
BINDIR		= ${DESTDIR}/usr/bin
PROG		= ydr

CFLAGS		+= -I${AFSSRC}/util -I${AFSSRC}/ydr -I.

.PATH:	${AFSSRC}/ydr ${AFSSRC}/util

beforedepend: parse.h

parse.h: y.tab.h
	-@ln -s y.tab.h parse.h

y.tab.h: parse.c

lex.o: parse.h

.if (${MACHINE_ARCH} == "m68k")
output.o: output.c
	${CC} -c ${CFLAGS} -O1 ${.IMPSRC}
.endif

.include <bsd.prog.mk>
