#    $Id: Makefile,v 1.2 1995/12/06 10:55:03 deraadt Exp $

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
