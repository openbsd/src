#	$OpenBSD: Makefile,v 1.1 2001/08/19 19:57:54 mickey Exp $

CFLAGS+=	-DLIBC_SCCS -DELFSIZE=32 -I${.CURDIR}/..

#OBJS=		crt0.o gcrt0.o
OBJS=	crt0.o
SRCS=	crt0.c
NOPROFILE=noprofile
NOPIC=	nopic
NOMAN=	noman

CLEANFILES+=	core a.out

all:	${OBJS}

crt0.o:	${SRCS}
	@echo ${COMPILE.c} -DCRT0 ${.CURDIR}/${SRCS} -o ${.TARGET}
	@${COMPILE.c} -DCRT0 ${.CURDIR}/${SRCS} -o ${.TARGET}.o
	@${LD} -x -r -o ${.TARGET} ${.TARGET}.o
	@rm -f ${.TARGET}.o

install:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 ${OBJS} \
		${DESTDIR}/usr/lib

.include <bsd.prog.mk>

