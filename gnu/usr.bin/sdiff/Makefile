#	$NetBSD: Makefile,v 1.6 1995/04/23 07:58:50 cgd Exp $

PROG=		sdiff
SRCS=		sdiff.c getopt.c getopt1.c version.c
CFLAGS+=	-I$(.CURDIR)/../diff \
		-DHAVE_CONFIG_H -DDIFF_PROGRAM=\"/usr/bin/diff\"

LDADD+=		-lgnumalloc
DPADD+=		/usr/lib/libgnumalloc.a

.include <bsd.prog.mk>
.PATH: $(.CURDIR)/../diff
