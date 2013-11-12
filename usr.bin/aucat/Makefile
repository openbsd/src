#	$OpenBSD: Makefile,v 1.19 2013/11/12 06:47:34 ratchov Exp $

PROG=	aucat
SRCS=	aucat.c abuf.c aparams.c aproc.c dev.c midi.c file.c headers.c \
	siofile.c miofile.c pipe.c wav.c dbg.c
MAN=	aucat.1
CFLAGS+= -Wall -Wstrict-prototypes -Wundef -DDEBUG -I${.CURDIR}/../../lib/libsndio
LDADD+=	-lsndio
.include <bsd.prog.mk>
