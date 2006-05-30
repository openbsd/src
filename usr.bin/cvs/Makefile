#	$OpenBSD: Makefile,v 1.27 2006/05/30 07:00:30 joris Exp $

PROG=	opencvs
MAN=   cvs.1 cvsignore.5 cvsrc.5 cvswrappers.5 cvsintro.7
CPPFLAGS+=-I${.CURDIR}

SRCS=	cvs.c add.c commit.c config.c checkout.c buf.c cmd.c date.y diff.c \
	diff3.c diff_internals.c entries.c fatal.c file.c getlog.c log.c \
	import.c remove.c repository.c rcs.c rcsnum.c rcstime.c root.c \
	status.c worklist.c util.c update.c xmalloc.c

CFLAGS+=-Wall
CFLAGS+=-Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=-Wmissing-declarations
CFLAGS+=-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=-Wsign-compare
DEBUG=	-g -ggdb
YFLAGS=

DPADD+= ${LIBZ}
LDADD+= -lz

INSTALL_STRIP=

.include <bsd.prog.mk>
