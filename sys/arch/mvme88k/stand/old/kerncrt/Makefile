#    $Id: Makefile,v 1.1.1.1 1997/03/03 19:30:46 rahnds Exp $

CFLAGS+=-I${.CURDIR}/../include -I${.CURDIR}/${MACHINE_ARCH}

.PATH:  ${.CURDIR}/${MACHINE_ARCH}

SRCS= kerncrt.c

OBJS=${SRCS:.c=.o}

all:	${OBJS}

kerncrt.o:  ${SRCS}
	${CC} ${CFLAGS} -c -O ${.ALLSRC}
	${LD} -x -r ${.TARGET}
	mv a.out ${.TARGET}

install:

lint tags:

.include <bsd.prog.mk>
