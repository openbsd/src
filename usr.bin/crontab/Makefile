#	$OpenBSD: Makefile,v 1.5 2005/12/19 19:12:17 millert Exp $

PROG=	crontab
SRCS=	crontab.c misc.c entry.c env.c
CFLAGS+=-I${.CURDIR} -I${.CURDIR}/../../usr.sbin/cron -DDEBUGGING=0
BINGRP =crontab
BINMODE=2555
MAN=	crontab.1 crontab.5

.PATH: ${.CURDIR}/../../usr.sbin/cron

.include <bsd.prog.mk>
