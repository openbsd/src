#	$OpenBSD: Makefile,v 1.2 2003/08/05 11:11:42 hin Exp $

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

CFLAGS		+= -I${AFSSRC}/util -I.

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
