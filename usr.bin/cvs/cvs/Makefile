# $Id: Makefile,v 1.8 2004/08/03 04:58:46 jfb Exp $

.PATH:	${.CURDIR}/..

PROG=cvs
MAN=cvs.1 cvsrc.5

SRCS= cvs.c add.c buf.c checkout.c commit.c diff.c entries.c file.c \
	getlog.c history.c hist.c init.c log.c proto.c rcs.c rcsnum.c \
	req.c resp.c root.c server.c sock.c status.c update.c util.c version.c

BINDIR=/usr/bin
BINOWN=root
BINGRP=_cvsd
BINMODE=2555

CFLAGS+= -Wall
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wsign-compare
CFLAGS+= -DCVS



.include <bsd.prog.mk>
