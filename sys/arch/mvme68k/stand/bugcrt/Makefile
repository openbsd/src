#	$OpenBSD: Makefile,v 1.3 1996/04/28 10:48:26 deraadt Exp $

CFLAGS+=-I${.CURDIR}/../../include -O2

.include "${MACHINE_ARCH}/Makefile.inc"

.PATH:  ${.CURDIR}/${MACHINE_ARCH}

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
