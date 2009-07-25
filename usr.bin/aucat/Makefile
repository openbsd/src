#	$OpenBSD: Makefile,v 1.9 2009/07/25 08:44:27 ratchov Exp $

PROG=	aucat
SRCS=	aucat.c abuf.c aparams.c aproc.c dev.c midi.c file.c headers.c \
	safile.c miofile.c sock.c pipe.c listen.c opt.c wav.c legacy.c 
MAN=	aucat.1 midicat.1
LINKS=  ${BINDIR}/aucat ${BINDIR}/midicat
CFLAGS+= -DDEBUG -Wall -Wstrict-prototypes -Wundef
LDADD+=	-lsndio
.include <bsd.prog.mk>
