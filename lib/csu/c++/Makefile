#	$OpenBSD: Makefile,v 1.2 1996/03/05 07:55:33 niklas Exp $

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
	install ${COPY} -o ${BINOWN} -g ${BINGRP} -m 444 ${OBJS} \
	    ${DESTDIR}/usr/lib

.include <bsd.prog.mk>
