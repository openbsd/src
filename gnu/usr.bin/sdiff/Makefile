#	$OpenBSD: Makefile,v 1.3 2003/06/26 17:22:26 millert Exp $
#	$NetBSD: Makefile,v 1.6 1995/04/23 07:58:50 cgd Exp $

PROG=		sdiff
SRCS=		sdiff.c version.c
CFLAGS+=	-I$(.CURDIR)/../diff \
		-DHAVE_CONFIG_H -DDIFF_PROGRAM=\"/usr/bin/diff\"

.include <bsd.prog.mk>
.PATH: $(.CURDIR)/../diff
