#    $Id: Makefile,v 1.1.1.1 1995/10/18 10:44:45 deraadt Exp $

CFLAGS+=-I${.CURDIR}/../../include

.include "${MACHINE_ARCH}/Makefile.inc"

.PATH:  ${.CURDIR}/${MACHINE_ARCH}

OBJS=bugcrt.o

CLEANFILES+=a.out

all:	${OBJS}

bugcrt.o: bugcrt.c
	${CC} ${CFLAGS} -c -O ${.ALLSRC}
	${LD} -x -r ${.TARGET}
	mv a.out ${.TARGET}

install:

lint tags:

.include <bsd.prog.mk>
