#	$OpenBSD: Makefile,v 1.16 2011/10/17 21:09:11 ratchov Exp $

PROG=	aucat
SRCS=	aucat.c abuf.c aparams.c aproc.c dev.c midi.c file.c headers.c \
	siofile.c miofile.c sock.c pipe.c listen.c opt.c wav.c dbg.c
MAN=	aucat.1
CFLAGS+= -Wall -Wstrict-prototypes -Wundef -DDEBUG -I${.CURDIR}/../../lib/libsndio
LDADD+=	-lsndio
.include <bsd.prog.mk>
