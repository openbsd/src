#	$OpenBSD: Makefile,v 1.5 2008/10/27 00:26:33 ratchov Exp $

PROG=	aucat
SRCS=	aucat.c abuf.c aparams.c aproc.c dev.c file.c headers.c \
	safile.c sock.c pipe.c listen.c wav.c legacy.c 
CFLAGS+= -DDEBUG -Wall -Wstrict-prototypes -Werror -Wundef
LDADD+=	-lsndio
.include <bsd.prog.mk>
