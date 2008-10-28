#	$OpenBSD: Makefile,v 1.7 2008/10/28 21:57:39 miod Exp $

PROG=	aucat
SRCS=	aucat.c abuf.c aparams.c aproc.c dev.c file.c headers.c \
	safile.c sock.c pipe.c listen.c wav.c legacy.c 
CFLAGS+= -DDEBUG -Wall -Wstrict-prototypes -Werror -Wundef
LDADD+=	-lsndio
.include <bsd.prog.mk>
