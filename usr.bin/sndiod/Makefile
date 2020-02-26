#	$OpenBSD: Makefile,v 1.6 2020/02/26 13:53:58 ratchov Exp $

PROG=	sndiod
SRCS=	abuf.c dev.c dev_sioctl.c dsp.c fdpass.c file.c listen.c \
	midi.c miofile.c opt.c siofile.c sndiod.c sock.c utils.c
MAN=	sndiod.8
CFLAGS+=-DDEBUG -I${.CURDIR}/../../lib/libsndio
COPTS+=	-Wall -Wstrict-prototypes -Wmissing-prototypes -Wpointer-arith -Wundef
LDADD+=	-lsndio
.include <bsd.prog.mk>
