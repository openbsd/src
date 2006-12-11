#	$OpenBSD: Makefile,v 1.1 2006/12/11 23:10:11 reyk Exp $

.PATH:		${.CURDIR}/..

PROG=		bgplg
SRCS=		bgplg.c misc.c
CFLAGS+=	-Wall
CFLAGS+=	-Wstrict-prototypes -Wmissing-prototypes
CLFAGS+=	-Wmissing-declarations -Wredundant-decls
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare
CFLAGS+= 	-I${.CURDIR}
LDSTATIC=	-static
MAN=		bgplg.8

BINDIR=		/var/www/cgi-bin
BINMODE=	000

.include <bsd.prog.mk>
