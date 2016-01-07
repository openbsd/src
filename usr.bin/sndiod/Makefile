#	$OpenBSD: Makefile,v 1.5 2016/01/07 07:41:01 ratchov Exp $

PROG=	sndiod
SRCS=	abuf.c dev.c dsp.c fdpass.c file.c listen.c midi.c miofile.c \
	opt.c siofile.c sndiod.c sock.c utils.c
MAN=	sndiod.8
CFLAGS+=-DDEBUG -I${.CURDIR}/../../lib/libsndio
COPTS+=	-Wall -Wstrict-prototypes -Wmissing-prototypes -Wpointer-arith -Wundef
LDADD+=	-lsndio
.include <bsd.prog.mk>
