#	$OpenBSD: Makefile,v 1.4 1996/05/10 18:39:12 deraadt Exp $

CFLAGS+=-I${.CURDIR}/../../include -O2

OBJS=bugcrt.o

CLEANFILES+=a.out

all:	${OBJS}

bugcrt.o: bugcrt.c
	${CC} ${CFLAGS} -c ${.ALLSRC}
	${LD} -x -r ${.TARGET}
	mv a.out ${.TARGET}

install:

lint tags:

.include <bsd.prog.mk>
