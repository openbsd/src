# $Id: Makefile,v 1.12 2004/12/09 20:03:27 jfb Exp $

.PATH:	${.CURDIR}/..

PROG=cvs
MAN=cvs.1 cvsrc.5

SRCS= cvs.c add.c annotate.c buf.c checkout.c commit.c diff.c entries.c \
	file.c getlog.c history.c hist.c init.c log.c logmsg.c proto.c \
	rcs.c rcsnum.c req.c resp.c root.c server.c sock.c status.c \
	update.c util.c version.c

BINDIR=/usr/bin
BINOWN=root
BINGRP=_cvsd
BINMODE=555

CFLAGS= -g -ggdb
CFLAGS+= -Wall
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
CFLAGS+= -DCVS

INSTALL_STRIP=


.include <bsd.prog.mk>
