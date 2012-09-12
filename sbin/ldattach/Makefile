#	$OpenBSD: Makefile,v 1.3 2012/09/12 09:19:54 haesbaert Exp $

PROG=		ldattach
SRCS=		ldattach.c atomicio.c
MAN=		ldattach.8

CFLAGS+=	-Wall -Werror
CDIAGFLAGS=
LDADD+=		-lutil

BINDIR=		/sbin

.include <bsd.prog.mk>
