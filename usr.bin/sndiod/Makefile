#	$OpenBSD: Makefile,v 1.3 2013/11/21 08:15:46 ratchov Exp $

PROG=	sndiod
SRCS=	abuf.c dev.c dsp.c file.c listen.c midi.c miofile.c \
	opt.c siofile.c sndiod.c sock.c utils.c
MAN=	sndiod.1
CFLAGS+=-DDEBUG -I${.CURDIR}/../../lib/libsndio
COPTS+=	-Wall -Wstrict-prototypes -Wmissing-prototypes -Wpointer-arith -Wundef
LDADD+=	-lsndio
.include <bsd.prog.mk>
