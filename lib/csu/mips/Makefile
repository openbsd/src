#	$OpenBSD: Makefile,v 1.9 1999/08/20 14:11:35 niklas Exp $
#	from: @(#)Makefile	8.1 (Berkeley) 6/1/93

CFLAGS+=	-DLIBC_SCCS -I${.CURDIR}
OBJS=		crt0.o gcrt0.o scrt0.o crtn.o
CLEANFILES+=	core a.out
SRCS=		crt0.c
DEPS=

all: ${OBJS}

crt0.o: ${SRCS} ${DEPS}
	@echo "${COMPILE.c} -DCRT0 -DDYNAMIC ${.CURDIR}/${SRCS} -o ${.TARGET}"
	@${COMPILE.c} -DCRT0 -DDYNAMIC ${.CURDIR}/${SRCS} -o ${.TARGET}.o
	@${LD} -x -r ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o

gcrt0.o: ${SRCS} ${DEPS}
	@echo "${COMPILE.c} -DMCRT0 ${.CURDIR}/${SRCS} -o ${.TARGET}"
	@${COMPILE.c} -DMCRT0 ${.CURDIR}/${SRCS} -o ${.TARGET}.o
	@${LD} -x -r ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o

scrt0.o: ${SRCS} ${DEPS}
	@echo "${COMPILE.c} -DSCRT0 ${.CURDIR}/${SRCS} -o ${.TARGET}"
	@${COMPILE.c} -DSCRT0 ${.CURDIR}/${SRCS} -o ${.TARGET}.o
	@${LD} -x -r ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o

install:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 ${OBJS} \
	    ${DESTDIR}/usr/lib

depend lint tags:

.include <bsd.prog.mk>
