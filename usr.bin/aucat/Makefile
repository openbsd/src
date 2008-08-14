#	$OpenBSD: Makefile,v 1.3 2008/08/14 09:58:55 ratchov Exp $

PROG=	aucat
SRCS=	aucat.c abuf.c aparams.c aproc.c dev.c file.c headers.c \
	dev_sun.c legacy.c
CFLAGS+= -DDEBUG -Wall -Wstrict-prototypes -Werror -Wundef

.include <bsd.prog.mk>
