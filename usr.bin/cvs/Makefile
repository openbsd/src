#	$OpenBSD: Makefile,v 1.18 2006/01/03 17:04:53 xsa Exp $

PROG=	cvs
MAN=	cvs.1 cvsignore.5 cvsrc.5 cvswrappers.5 cvsintro.7
CPPFLAGS+=-I${.CURDIR}

SRCS=	cvs.c add.c admin.c annotate.c buf.c checkout.c cmd.c commit.c \
	compress.c date.y diff.c diff3.c edit.c entries.c fatal.c file.c \
	getlog.c history.c hist.c import.c init.c log.c logmsg.c proto.c \
	rcs.c rcsnum.c release.c remove.c req.c resp.c root.c server.c \
	status.c tag.c update.c util.c version.c watch.c xmalloc.c

CFLAGS= -g -ggdb
CFLAGS+= -Wall
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
YFLAGS=

DPADD+= ${LIBZ}
LDADD+= -lz

INSTALL_STRIP=

.include <bsd.prog.mk>
