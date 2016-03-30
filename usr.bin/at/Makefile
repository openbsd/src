#	$OpenBSD: Makefile,v 1.12 2016/03/30 06:38:45 jmc Exp $

PROG=	at
SRCS=	at.c client.c parsetime.c
CRONDIR=${.CURDIR}/../../usr.sbin/cron
CFLAGS+=-I${CRONDIR}
MAN=	at.1 atrm.1 atq.1
LINKS=	${BINDIR}/at ${BINDIR}/atq \
	${BINDIR}/at ${BINDIR}/atrm \
	${BINDIR}/at ${BINDIR}/batch
BINGRP=	crontab
BINMODE= 2555

.PATH: ${CRONDIR}

.include <bsd.prog.mk>
