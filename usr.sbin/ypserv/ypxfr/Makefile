#	$OpenBSD: Makefile,v 1.4 2001/11/07 19:01:38 deraadt Exp $

PROG=	ypxfr
SRCS=	ypxfr.c ypdb.c yplog.c yplib_host.c
MAN=	ypxfr.8
LDADD=	-lrpcsvc
DPADD=	${LIBRPCSVC}
MLINKS= ypxfr.8 ypxfr_1perhour.8 \
	ypxfr.8 ypxfr_2perday.8 \
	ypxfr.8 ypxfr_1perday.8
SCRIPTS= ypxfr_1perhour.sh ypxfr_2perday.sh ypxfr_1perday.sh

.PATH:	${.CURDIR}/../common
CFLAGS+=-I${.CURDIR}/../common

beforeinstall:
.for script in ${SCRIPTS}
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/${script} ${DESTDIR}${BINDIR}/${script:R}
.endfor

.include <bsd.prog.mk>
