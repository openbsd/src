#	$OpenBSD: Makefile,v 1.5 2013/12/03 06:21:41 guenther Exp $

#CFLAGS+=	-I${.CURDIR} -mips2
CFLAGS+=	-I${.CURDIR}
OBJS=		crt0.o gcrt0.o crtbegin.o crtend.o crtbeginS.o crtendS.o

.PATH: ${.CURDIR}/../common_elf

all: ${OBJS}

crt0.o: crt0.c ${DEPS}
	@echo "${COMPILE.c} -DCRT0 -c ${.ALLSRC} -o ${.TARGET}"
	@${COMPILE.c} -DCRT0 -c ${.ALLSRC} -o ${.TARGET}.o
	@${LD} -x -r ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o

gcrt0.o: crt0.c ${DEPS}
	@echo "${COMPILE.c} -DMCRT0 -c ${.ALLSRC} -o ${.TARGET}"
	@${COMPILE.c} -DMCRT0 -c ${.ALLSRC} -o ${.TARGET}.o
	@${LD} -x -r ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o

realinstall:
	${INSTALL} ${INSTALL_COPY} -S -o ${BINOWN} -g ${BINGRP} -m 444 ${OBJS} \
	    ${DESTDIR}/usr/lib

depend lint tags:

.include <bsd.prog.mk>
