#	$OpenBSD: Makefile,v 1.2 2002/11/10 22:49:54 mickey Exp $
#	from: @(#)Makefile	5.5 (Berkeley) 5/21/91

CFLAGS+=	-DLIBC_SCCS -I${.CURDIR}/..
OBJS=		crt0.o scrt0.o gcrt0.o crtbegin.o crtend.o crtbeginS.o crtendS.o

.PATH:		${.CURDIR}/../common_elf
CFLAGS+=	-I${.CURDIR}/../common_elf

#PICFLAG?=-fPIC

all: ${OBJS}

crt0.o: crt0.c
	${COMPILE.c} -DCRT0 ${.ALLSRC} -o ${.TARGET}.o
	@${LD} -x -r -o ${.TARGET} ${.TARGET}.o
	@rm -f ${.TARGET}.o

scrt0.o: crt0.c
	${COMPILE.c} -DSCRT0 ${.ALLSRC} -o ${.TARGET}.o
	@${LD} -x -r -o ${.TARGET} ${.TARGET}.o
	@rm -f ${.TARGET}.o

gcrt0.o: crt0.c
	${COMPILE.c} -DMCRT0 ${.ALLSRC} -o ${.TARGET}.o
	@${LD} -x -r -o ${.TARGET} ${.TARGET}.o
	@rm -f ${.TARGET}.o

crtbegin.o: crtbegin.c
	${COMPILE.c} ${.ALLSRC} -o ${.TARGET}.o
	@${LD} -x -r -o ${.TARGET} ${.TARGET}.o
	@rm -f ${.TARGET}.o

crtbeginS.o: crtbeginS.c
	${COMPILE.c} ${PICFLAG} ${.ALLSRC} -o ${.TARGET}.o
	@${LD} -x -r -o ${.TARGET} ${.TARGET}.o
	@rm -f ${.TARGET}.o

crtend.o: crtend.c
	${COMPILE.c} ${.ALLSRC} -o ${.TARGET}.o
	@${LD} -x -r -o ${.TARGET} ${.TARGET}.o
	@rm -f ${.TARGET}.o

crtendS.o: crtendS.c
	${COMPILE.c} ${PICFLAG} ${.ALLSRC} -o ${.TARGET}.o
	@${LD} -x -r -o ${.TARGET} ${.TARGET}.o
	@rm -f ${.TARGET}.o

realinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 ${OBJS} \
	    ${DESTDIR}/usr/lib

.include <bsd.prog.mk>
