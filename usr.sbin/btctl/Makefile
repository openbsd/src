#	$OpenBSD: Makefile,v 1.2 2008/11/25 17:13:53 uwe Exp $

.PATH:		${.CURDIR}/../btd

PROG=		btctl
SRCS=		bt_subr.c btctl.c conf.c log.c parse.y
NOMAN=

LDADD+=		-lusbhid
CPPFLAGS+=	-I${.CURDIR} -I${.CURDIR}/../btd
COPTS+=		-Wall -Werror
DEBUG=		-g

.include <bsd.prog.mk>
