#	$OpenBSD: Makefile,v 1.4 1996/08/22 20:33:14 millert Exp $

PROG=	rdist
CFLAGS+=-I${.CURDIR}
SRCS=	docmd.c expand.c lookup.c main.c server.c
OBJS+=	gram.o
BINOWN=	root
BINMODE=555
CLEANFILES=y.tab.h
MAN=	oldrdist.1

realinstall:
	install ${COPY} ${STRIP} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${PROG} ${DESTDIR}${BINDIR}/oldrdist

.include <bsd.prog.mk>
