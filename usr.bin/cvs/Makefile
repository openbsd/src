#	$OpenBSD: Makefile,v 1.47 2008/06/21 15:39:15 joris Exp $

PROG=	opencvs
MAN=	cvsintro.7 # cvs.1 cvs.5
CPPFLAGS+=-I${.CURDIR}

SRCS=	cvs.c add.c admin.c annotate.c atomicio.c commit.c config.c \
	checkout.c client.c buf.c cmd.c date.y diff.c diff3.c \
	diff_internals.c edit.c entries.c fatal.c file.c getlog.c hash.c \
	hash_func.c history.c log.c logmsg.c modules.c import.c init.c  \
	release.c remove.c repository.c rcs.c rcsnum.c remote.c root.c \
	server.c status.c tag.c trigger.c worklist.c util.c update.c version.c \
	watch.c xmalloc.c

CFLAGS+=-Wall
CFLAGS+=-Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=-Wmissing-declarations
CFLAGS+=-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=-Wsign-compare
DEBUG=	-g -ggdb
YFLAGS=

INSTALL_STRIP=

.include <bsd.prog.mk>
