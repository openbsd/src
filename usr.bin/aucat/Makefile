#	$OpenBSD: Makefile,v 1.22 2015/01/21 08:43:55 ratchov Exp $

PROG=	aucat
SRCS=	abuf.c afile.c aucat.c dsp.c utils.c
MAN=	aucat.1
CFLAGS+=-DDEBUG -I${.CURDIR}/../../lib/libsndio
COPTS+=	-Wall -Wstrict-prototypes -Wmissing-prototypes -Wpointer-arith -Wundef
LDADD+=	-lsndio
.include <bsd.prog.mk>
