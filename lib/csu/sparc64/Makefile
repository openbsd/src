#	$OpenBSD: Makefile,v 1.1 2001/08/29 02:26:09 art Exp $
#	$NetBSD: Makefile,v 1.6 1996/10/18 05:27:38 thorpej Exp $

CFLAGS+=	-DLIBC_SCCS -DPIC -DELFSIZE=64

OBJS=		crt0.o gcrt0.o crtbegin.o crtend.o crtbeginS.o crtendS.o

CLEANFILES+=	core a.out

.PATH:		${.CURDIR}/../common_elf
CFLAGS+=	-I${.CURDIR}/../common_elf

PICFLAG?=-fpic

all: ${OBJS}

crt0.o: crt0.c
	${COMPILE.c} -DCRT0 ${.ALLSRC} -o ${.TARGET}.o
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
