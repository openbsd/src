# $Id: Makefile,v 1.1.1.1 2004/07/13 22:02:40 jfb Exp $

.PATH:	${.CURDIR}/..

PROG=ocvs
BINDIR=/usr/bin
BINOWN=root
BINGRP=_cvsd
BINMODE=2555
MAN=cvs.1 cvsrc.5

SRCS= cvs.c add.c buf.c client.c commit.c diff.c entries.c getlog.c \
	history.c hist.c init.c lock.c log.c proto.c rcs.c rcsnum.c root.c \
	server.c sock.c update.c util.c version.c

.include <bsd.prog.mk>
