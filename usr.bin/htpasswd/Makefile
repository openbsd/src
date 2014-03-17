#	$OpenBSD: Makefile,v 1.1 2014/03/17 12:49:13 florian Exp $
PROG=		htpasswd
SRCS=		htpasswd.c
MAN=		htpasswd.1

CFLAGS+=	-g -W -Wall -Werror
CFLAGS+=	-Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual -Wsign-compare

.include <bsd.prog.mk>
