#	$OpenBSD: Makefile,v 1.3 2017/04/16 10:19:54 jsg Exp $
PROG=		htpasswd
SRCS=		htpasswd.c
MAN=		htpasswd.1

CFLAGS+=	-W -Wall
CFLAGS+=	-Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual -Wsign-compare

.include <bsd.prog.mk>
