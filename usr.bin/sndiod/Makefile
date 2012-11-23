#	$OpenBSD: Makefile,v 1.1 2012/11/23 07:03:28 ratchov Exp $

PROG=	sndiod
SRCS=	abuf.c dev.c dsp.c file.c listen.c midi.c miofile.c opt.c siofile.c sndiod.c sock.c utils.c
MAN=	sndiod.1
CFLAGS+= -Wall -Wstrict-prototypes -Wundef -DDEBUG -I${.CURDIR}/../../lib/libsndio
LDADD+=	-lsndio
.include <bsd.prog.mk>
