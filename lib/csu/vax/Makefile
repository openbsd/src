#	$OpenBSD: Makefile,v 1.7 2009/10/27 23:59:59 deraadt Exp $
#	from: @(#)Makefile	5.6 (Berkeley) 5/22/91

CFLAGS+=	-I${.CURDIR}/..
OBJS=		crt0.o gcrt0.o scrt0.o
CLEANFILES+=	core crt0.out gcrt0.out scrt0.out

all: ${OBJS}

crt0.o: crt0.c
	${COMPILE.c} -DCRT0 -UDYNAMIC ${.ALLSRC}
	${LD} -x -r ${.TARGET} -o ${.TARGET}ut
	mv ${.TARGET}ut ${.TARGET}

gcrt0.o: crt0.c
	${COMPILE.c} -DMCRT0 ${.ALLSRC} -o ${.TARGET}
	${LD} -x -r ${.TARGET} -o ${.TARGET}ut
	mv ${.TARGET}ut ${.TARGET}

scrt0.o: crt0.c
	${COMPILE.c} -DSCRT0 ${.ALLSRC} -o ${.TARGET}
	${LD} -x -r ${.TARGET} -o ${.TARGET}ut
	mv ${.TARGET}ut ${.TARGET}

realinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 ${OBJS} \
	    ${DESTDIR}/usr/lib

depend lint tags:

.include <bsd.prog.mk>
