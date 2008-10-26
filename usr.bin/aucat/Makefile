#	$OpenBSD: Makefile,v 1.4 2008/10/26 08:49:43 ratchov Exp $

PROG=	aucat
SRCS=	aucat.c abuf.c aparams.c aproc.c dev.c file.c headers.c \
	safile.c sock.c pipe.c listen.c wav.c legacy.c 
CFLAGS+= -DDEBUG -Wall -Wstrict-prototypes -Werror -Wundef
LDADD+=	-lsa
.include <bsd.prog.mk>
