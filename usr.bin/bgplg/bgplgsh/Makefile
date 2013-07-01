#	$OpenBSD: Makefile,v 1.2 2013/07/01 10:15:04 kettenis Exp $

.PATH:		${.CURDIR}/..

PROG=		bgplgsh
SRCS=		bgplgsh.c misc.c
CFLAGS+=	-Wall
CFLAGS+=	-Wstrict-prototypes -Wmissing-prototypes
CLFAGS+=	-Wmissing-declarations -Wredundant-decls
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare
CPPFLAGS+= 	-I${.CURDIR}
CPPFLAGS+=	-I${.CURDIR}/../../lib/libedit
LDADD=		-ledit -lncurses
MAN=		bgplgsh.8

BINDIR=		/usr/bin

.include <bsd.prog.mk>
