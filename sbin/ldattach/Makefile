#	$OpenBSD: Makefile,v 1.4 2017/04/16 10:16:35 jsg Exp $

PROG=		ldattach
SRCS=		ldattach.c atomicio.c
MAN=		ldattach.8

CFLAGS+=	-Wall
CDIAGFLAGS=
LDADD+=		-lutil

BINDIR=		/sbin

.include <bsd.prog.mk>
