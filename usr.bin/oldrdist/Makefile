#	from: @(#)Makefile	5.11 (Berkeley) 3/12/91
#	$Id: Makefile,v 1.1 1996/02/03 12:11:51 dm Exp $

PROG=	rdist
CFLAGS+=-I${.CURDIR}
SRCS=	docmd.c expand.c lookup.c main.c server.c
OBJS+=	gram.o
BINOWN=	root
BINMODE=4555
CLEANFILES=y.tab.h
MAN=	oldrdist.1

LDADD=	-lcompat
DPADD=	${LIBCOMPAT}

realinstall:
	install ${COPY} ${STRIP} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${PROG} ${DESTDIR}${BINDIR}/oldrdist

.include <bsd.prog.mk>
