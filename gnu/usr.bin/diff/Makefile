#	$NetBSD: Makefile,v 1.8 1995/07/12 07:21:05 cgd Exp $

PROG=	diff
SRCS=	diff.c analyze.c cmpbuf.c io.c context.c ed.c normal.c util.c dir.c \
	regex.c side.c ifdef.c version.c getopt.c getopt1.c

CFLAGS+=-DHAVE_CONFIG_H -I${.CURDIR} -DPR_PROGRAM=\"/usr/bin/pr\"
LDADD+=	-lgnumalloc
DPADD+=	/usr/lib/libgnumalloc.a

.include <bsd.prog.mk>
