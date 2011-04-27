#	$OpenBSD: Makefile,v 1.15 2011/04/27 21:20:36 ratchov Exp $

PROG=	aucat
SRCS=	aucat.c abuf.c aparams.c aproc.c dev.c midi.c file.c headers.c \
	siofile.c miofile.c sock.c pipe.c listen.c opt.c wav.c dbg.c
MAN=	aucat.1 midicat.1
LINKS=  ${BINDIR}/aucat ${BINDIR}/midicat
CFLAGS+= -Wall -Wstrict-prototypes -Wundef -DDEBUG -I${.CURDIR}/../../lib/libsndio
LDADD+=	-lsndio
.include <bsd.prog.mk>
