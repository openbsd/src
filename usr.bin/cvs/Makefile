#	$OpenBSD: Makefile,v 1.7 2005/05/17 16:45:07 xsa Exp $

PROG=	cvs
MAN=	cvs.1 cvsrc.5 cvsintro.7

SRCS=	cvs.c add.c admin.c annotate.c buf.c checkout.c cmd.c commit.c date.y \
	diff.c entries.c file.c getlog.c history.c hist.c import.c init.c \
	log.c logmsg.c proto.c rcs.c rcsnum.c release.c remove.c req.c resp.c \
	root.c server.c status.c strtab.c tag.c update.c util.c version.c

BINOWN=	root

#BINGRP=_cvsd
#BINMODE=555

CFLAGS= -g -ggdb
CFLAGS+= -I${.CURDIR} -Wall
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
CFLAGS+= -DCVS
YFLAGS=

INSTALL_STRIP=

.include <bsd.prog.mk>
