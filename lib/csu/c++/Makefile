#	$OpenBSD: Makefile,v 1.6 1998/09/15 11:01:51 pefo Exp $

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
