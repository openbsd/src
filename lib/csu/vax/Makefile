#	from: @(#)Makefile	5.6 (Berkeley) 5/22/91
#	$Id: Makefile,v 1.1 1995/11/01 17:38:00 deraadt Exp $

CFLAGS+=	-DLIBC_SCCS -I${.CURDIR}/..
OBJS=		crt0.o gcrt0.o scrt0.o
CLEANFILES+=	core a.out

all: ${OBJS}

crt0.o: crt0.c
	${COMPILE.c} -DCRT0 -UDYNAMIC ${.ALLSRC}
	${LD} -x -r ${.TARGET}
	mv a.out ${.TARGET}

gcrt0.o: crt0.c
	${COMPILE.c} -DMCRT0 ${.ALLSRC} -o ${.TARGET}
	${LD} -x -r ${.TARGET}
	mv a.out ${.TARGET}

scrt0.o: crt0.c
	${COMPILE.c} -DSCRT0 ${.ALLSRC} -o ${.TARGET}
	${LD} -x -r ${.TARGET}
	mv a.out ${.TARGET}

install:
	install ${COPY} -o ${BINOWN} -g ${BINGRP} -m 444 ${OBJS} \
	    ${DESTDIR}/usr/lib

depend lint tags:

.include <bsd.prog.mk>
