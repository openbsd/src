#	$OpenBSD: Makefile,v 1.4 2002/12/09 23:43:11 millert Exp $
#	$NetBSD: Makefile,v 1.8 1995/07/12 07:21:05 cgd Exp $

PROG=	diff
SRCS=	diff.c analyze.c cmpbuf.c io.c context.c ed.c normal.c util.c dir.c \
	regex.c side.c ifdef.c version.c

CFLAGS+=-DHAVE_CONFIG_H -I${.CURDIR} -DPR_PROGRAM=\"/usr/bin/pr\"

.include <bsd.prog.mk>
