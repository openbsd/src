#	$OpenBSD: Makefile,v 1.10 2009/09/27 11:51:20 ratchov Exp $

PROG=	aucat
SRCS=	aucat.c abuf.c aparams.c aproc.c dev.c midi.c file.c headers.c \
	safile.c miofile.c sock.c pipe.c listen.c opt.c wav.c legacy.c 
MAN=	aucat.1 midicat.1
LINKS=  ${BINDIR}/aucat ${BINDIR}/midicat
CFLAGS+= -Wall -Wstrict-prototypes -Wundef
LDADD+=	-lsndio
.include <bsd.prog.mk>
