#	$OpenBSD: Makefile,v 1.6 2015/01/23 01:58:20 tedu Exp $

PROG=	crontab
SRCS=	crontab.c misc.c entry.c env.c
CFLAGS+=-I${.CURDIR} -I${.CURDIR}/../../usr.sbin/cron
BINGRP =crontab
BINMODE=2555
MAN=	crontab.1 crontab.5

.PATH: ${.CURDIR}/../../usr.sbin/cron

.include <bsd.prog.mk>
