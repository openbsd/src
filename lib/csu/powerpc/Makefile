#	$OpenBSD: Makefile,v 1.4 1999/08/20 14:11:36 niklas Exp $
#	from: @(#)Makefile	8.1 (Berkeley) 6/1/93

CFLAGS=		-DLIBC_SCCS
OBJS=		crt0.o gcrt0.o crtbegin.o crtend.o
CLEANFILES+=	core a.out

all: ${OBJS}

crt0.o: crt0.s
	${CPP} -DCRT0 ${.ALLSRC} | ${AS} -o $@
	@${LD} -x -r ${.TARGET}
	@mv a.out ${.TARGET}

gcrt0.o: crt0.s
	${CPP} -DMCRT0 ${.ALLSRC} | ${AS} -o $@
	@${LD} -x -r ${.TARGET}
	@mv a.out ${.TARGET}

crtbegin.o: crtbegin.c
	@echo ${CC} ${CFLAGS} -c ${.ALLSRC} -o ${.TARGET}
	@${CC} ${CFLAGS} -c ${.ALLSRC} -o ${.TARGET}.o
	@${LD} -x -r -o ${.TARGET} ${.TARGET}.o
	@rm -f ${.TARGET}.o
 
crtend.o: crtend.c
	@echo ${CC} ${CFLAGS} -c ${.ALLSRC} -o ${.TARGET}
	@${CC} ${CFLAGS} -c ${.ALLSRC} -o ${.TARGET}.o
	@${LD} -x -r -o ${.TARGET} ${.TARGET}.o
	@rm -f ${.TARGET}.o

install:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 ${OBJS} \
		${DESTDIR}/usr/lib

depend lint tags:

.include <bsd.prog.mk>
