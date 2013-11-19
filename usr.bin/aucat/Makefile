#	$OpenBSD: Makefile,v 1.20 2013/11/19 07:24:57 ratchov Exp $

PROG=	aucat
SRCS=	aucat.c abuf.c aparams.c aproc.c dev.c midi.c file.c headers.c \
	siofile.c miofile.c pipe.c wav.c dbg.c
MAN=	aucat.1
CFLAGS+=-DDEBUG -I${.CURDIR}/../../lib/libsndio
COPTS+=	-Wall -Wstrict-prototypes -Wpointer-arith -Wundef
LDADD+=	-lsndio
.include <bsd.prog.mk>
