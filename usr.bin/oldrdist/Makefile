#	$OpenBSD: Makefile,v 1.5 1996/12/08 14:32:31 downsj Exp $

PROG=	rdist
CFLAGS+=-I${.CURDIR}
SRCS=	docmd.c expand.c lookup.c main.c server.c
OBJS+=	gram.o
BINOWN=	root
BINMODE=555
CLEANFILES=y.tab.h
MAN=	oldrdist.1

realinstall:
	${INSTALL} ${COPY} ${STRIP} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${PROG} ${DESTDIR}${BINDIR}/oldrdist

.include <bsd.prog.mk>
