#	$OpenBSD: Makefile,v 1.2 2014/12/23 19:32:16 pascal Exp $

.PATH:		${.CURDIR}/..

PROG=		bgplg
SRCS=		bgplg.c misc.c
CFLAGS+=	-Wall
CFLAGS+=	-Wstrict-prototypes -Wmissing-prototypes
CLFAGS+=	-Wmissing-declarations -Wredundant-decls
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare
CFLAGS+= 	-I${.CURDIR}
LDSTATIC=	${STATIC}
MAN=		bgplg.8

BINDIR=		/var/www/cgi-bin
BINMODE=	000

.include <bsd.prog.mk>
