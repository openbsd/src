#	$OpenBSD: Makefile,v 1.17 2011/12/09 14:36:42 ratchov Exp $

PROG=	aucat
SRCS=	aucat.c abuf.c aparams.c aproc.c dev.c midi.c file.c headers.c \
	siofile.c miofile.c sock.c pipe.c listen.c opt.c wav.c dbg.c
MAN=	aucat.1
CFLAGS+= -Wall -Wstrict-prototypes -Wundef -DDEBUG -I${.CURDIR}/../../lib/libsndio
LDADD+=	-lsndio
LINKS=  ${BINDIR}/aucat ${BINDIR}/sndiod
MLINKS = aucat.1 sndiod.1
.include <bsd.prog.mk>
