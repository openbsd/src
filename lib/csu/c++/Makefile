#	$OpenBSD: Makefile,v 1.5 1997/10/07 22:38:31 niklas Exp $

.include <bsd.own.mk>

CFLAGS+=	-DLIBC_SCCS
.if !defined(NOPIC)
CFLAGS+=	-fpic
.endif
OBJS=		c++rt0.o

all: ${OBJS}

c++rt0.o: c++rt0.c
	${COMPILE.c} ${.ALLSRC}
	@${LD} -x -r ${.TARGET}
	@mv a.out ${.TARGET}

install:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 ${OBJS} \
	    ${DESTDIR}/usr/lib

.include <bsd.prog.mk>
