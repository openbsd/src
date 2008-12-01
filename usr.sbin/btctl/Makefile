#	$OpenBSD: Makefile,v 1.3 2008/12/01 19:02:25 todd Exp $

.PATH:		${.CURDIR}/../btd

PROG=		btctl
SRCS=		bt_subr.c btctl.c conf.c log.c parse.y
MAN=		bt.conf.5

LDADD+=		-lusbhid
CPPFLAGS+=	-I${.CURDIR} -I${.CURDIR}/../btd
COPTS+=		-Wall -Werror
DEBUG=		-g

.include <bsd.prog.mk>
