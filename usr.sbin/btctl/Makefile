#	$OpenBSD: Makefile,v 1.1 2008/11/24 23:34:41 uwe Exp $

.PATH:		${.CURDIR}/../btd

PROG=		btctl
SRCS=		btctl.c conf.c log.c parse.y
NOMAN=

LDADD+=		-lbluetooth -lsdp -lusbhid
LDFLAGS+=	-L/usr/local/lib
CPPFLAGS+=	-I${.CURDIR} -I${.CURDIR}/../btd -I/usr/local/include
COPTS+=		-Wall -Werror
DEBUG=		-g

.include <bsd.prog.mk>
