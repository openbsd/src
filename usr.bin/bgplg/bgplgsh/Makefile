#	$OpenBSD: Makefile,v 1.3 2022/09/03 15:58:40 claudio Exp $

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
LDADD=		-ledit
DPADD+=		${LIBEDIT}
MAN=		bgplgsh.8

BINDIR=		/usr/bin

.include <bsd.prog.mk>
