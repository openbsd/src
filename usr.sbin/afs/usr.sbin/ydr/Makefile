#	$OpenBSD: Makefile,v 1.1 2002/06/07 05:02:59 hin Exp $

AFSTOPDIR=	../..
.include <../../Makefile.inc>

SRCS		+= main.c output.c symbol.c types.c lex.c parse.c list.c hash.c
CLEANFILES	+= lex.c parse.c parse.h y.tab.h
CFLAGS		+= -DCPP='"/usr/bin/cpp"'
MAN		=
BINDIR		= ${DESTDIR}/usr/bin
PROG		= ydr
LDADD		+= ${LIBROKEN}
DPADD		+= ${LIBROKEN}

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
