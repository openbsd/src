#	$OpenBSD: Makefile,v 1.9 2015/01/23 01:58:20 tedu Exp $

PROG=	at
SRCS=	at.c misc.c parsetime.c
CRONDIR=${.CURDIR}/../../usr.sbin/cron
CFLAGS+=-I${CRONDIR}
MAN=	at.1 atrm.1 atq.1
LINKS=	${BINDIR}/at ${BINDIR}/atq \
	${BINDIR}/at ${BINDIR}/atrm \
	${BINDIR}/at ${BINDIR}/batch
MLINKS=	at.1 batch.1
BINGRP=	crontab
BINMODE= 2555

.PATH: ${CRONDIR}

.include <bsd.prog.mk>
