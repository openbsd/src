#	$OpenBSD: Makefile,v 1.6 1997/04/27 20:56:51 millert Exp $

PROG=	rdist
CFLAGS+=-I${.CURDIR}
SRCS=	docmd.c expand.c lookup.c main.c server.c
OBJS+=	gram.o
BINOWN=	root
BINMODE=555
CLEANFILES=y.tab.h
MAN=	oldrdist.1

realinstall:
	${INSTALL} ${INSTALL_COPY} ${INSTALL_STRIP} -o ${BINOWN} -g ${BINGRP} \
	    -m ${BINMODE} ${PROG} ${DESTDIR}${BINDIR}/oldrdist

.include <bsd.prog.mk>
