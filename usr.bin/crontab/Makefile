#	$OpenBSD: Makefile,v 1.7 2015/10/31 12:19:41 millert Exp $

PROG=	crontab
SRCS=	crontab.c entry.c env.c client.c common.c misc.c
CFLAGS+=-I${.CURDIR} -I${.CURDIR}/../../usr.sbin/cron
BINGRP =crontab
BINMODE=2555
MAN=	crontab.1 crontab.5

.PATH: ${.CURDIR}/../../usr.sbin/cron

.include <bsd.prog.mk>
