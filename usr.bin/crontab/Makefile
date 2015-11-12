#	$OpenBSD: Makefile,v 1.8 2015/11/12 21:12:05 millert Exp $

PROG=	crontab
SRCS=	crontab.c entry.c env.c client.c misc.c
CFLAGS+=-I${.CURDIR} -I${.CURDIR}/../../usr.sbin/cron
BINGRP =crontab
BINMODE=2555
MAN=	crontab.1 crontab.5

.PATH: ${.CURDIR}/../../usr.sbin/cron

.include <bsd.prog.mk>
