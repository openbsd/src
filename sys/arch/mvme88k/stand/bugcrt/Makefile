#    $Id: Makefile,v 1.3 1997/03/03 20:22:14 rahnds Exp $

CFLAGS+=-I${.CURDIR}/../../include 

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
