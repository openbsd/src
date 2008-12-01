#	$OpenBSD: Makefile,v 1.4 2008/12/01 20:22:56 todd Exp $

.PATH:		${.CURDIR}/../btd

PROG=		btctl
SRCS=		bt_subr.c btctl.c conf.c log.c parse.y
MAN=		btctl.8 bt.conf.5

LDADD+=		-lusbhid
CPPFLAGS+=	-I${.CURDIR} -I${.CURDIR}/../btd
COPTS+=		-Wall -Werror
DEBUG=		-g

.include <bsd.prog.mk>
