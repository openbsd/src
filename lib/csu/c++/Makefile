#	$OpenBSD: Makefile,v 1.3 1996/12/08 15:22:18 downsj Exp $

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
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 444 ${OBJS} \
	    ${DESTDIR}/usr/lib

.include <bsd.prog.mk>
