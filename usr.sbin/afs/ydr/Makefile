#	$OpenBSD: Makefile,v 1.3 1999/05/26 13:52:58 espie Exp $
SRCS		+= main.c output.c symbol.c types.c lex.c parse.c
CLEANFILES	+= lex.c parse.c parse.h y.tab.h
CFLAGS		+= -I. -I${.CURDIR}/../src/util -I${.CURDIR}/../src/lib/roken \
		  -I${.CURDIR}/../src/include -I${.CURDIR} -DHAVE_CONFIG_H
CFLAGS		+= -DCPP='"/usr/bin/cpp"'
MAN		=
BINDIR		= ${DESTDIR}/usr/bin
PROG		= ydr
LDADD		+= ../libroken/libroken.a
DPADD		+= ../libroken/libroken.a

.PATH:	${.CURDIR}/../src/ydr

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
