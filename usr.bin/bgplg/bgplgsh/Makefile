#	$OpenBSD: Makefile,v 1.4 2025/07/12 05:28:33 miod Exp $

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
LDADD=		-ledit -lcurses
DPADD+=		${LIBEDIT} ${LIBCURSES}
MAN=		bgplgsh.8

BINDIR=		/usr/bin

.include <bsd.prog.mk>
