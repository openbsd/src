#	$OpenBSD: Makefile,v 1.1.1.1 1998/09/14 21:53:30 art Exp $
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

.include <bsd.prog.mk>