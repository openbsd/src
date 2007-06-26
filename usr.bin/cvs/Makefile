#	$OpenBSD: Makefile,v 1.40 2007/06/26 18:02:43 xsa Exp $

PROG=	opencvs
MAN=	cvs.1 cvs.5 cvsintro.7
CPPFLAGS+=-I${.CURDIR}

SRCS=	cvs.c add.c admin.c annotate.c commit.c config.c checkout.c client.c \
	buf.c cmd.c date.y diff.c diff3.c diff_internals.c edit.c entries.c \
	fatal.c file.c getlog.c history.c log.c logmsg.c import.c init.c \
	release.c remove.c repository.c rcs.c rcsnum.c remote.c root.c \
	server.c status.c tag.c worklist.c util.c update.c version.c \
	watch.c xmalloc.c

CFLAGS+=-Wall
CFLAGS+=-Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=-Wmissing-declarations
CFLAGS+=-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=-Wsign-compare
DEBUG=	-g -ggdb
YFLAGS=

DPADD+=	${LIBZ}
LDADD+=	-lz

INSTALL_STRIP=

.include <bsd.prog.mk>
