#	$OpenBSD: Makefile,v 1.6 2014/05/12 19:11:20 espie Exp $

PROG =	tsort
SRCS = tsort.c

CDIAGFLAGS = -Wall -Wno-char-subscripts -Wstrict-prototypes -pedantic -W
DPADD += ${LIBUTIL}
LDADD += -lutil

.include <bsd.prog.mk>
