#	$OpenBSD: Makefile,v 1.2 2008/05/23 07:15:46 ratchov Exp $

PROG=	aucat
SRCS=	aucat.c abuf.c aparams.c aproc.c dev_sun.c file.c headers.c \
	legacy.c
CFLAGS+= -DDEBUG -Wall -Wstrict-prototypes -Werror -Wundef

.include <bsd.prog.mk>
