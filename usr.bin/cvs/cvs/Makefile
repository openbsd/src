# $Id: Makefile,v 1.9 2004/11/18 15:48:53 jfb Exp $

.PATH:	${.CURDIR}/..

PROG=cvs
MAN=cvs.1 cvsrc.5

SRCS= cvs.c add.c buf.c checkout.c commit.c diff.c entries.c file.c \
	getlog.c history.c hist.c init.c log.c logmsg.c proto.c rcs.c rcsnum.c \
	req.c resp.c root.c server.c sock.c status.c update.c util.c version.c

BINDIR=/usr/bin
BINOWN=root
BINGRP=_cvsd
BINMODE=555

CFLAGS+= -Wall
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wsign-compare
CFLAGS+= -DCVS



.include <bsd.prog.mk>
