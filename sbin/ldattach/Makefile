#	$OpenBSD: Makefile,v 1.2 2008/06/09 21:06:10 mbalmer Exp $

PROG=		ldattach
SRCS=		ldattach.c atomicio.c
MAN=		ldattach.8

CFLAGS+=	-Wall -Werror
LDADD+=		-lutil

BINDIR=		/sbin

.include <bsd.prog.mk>
