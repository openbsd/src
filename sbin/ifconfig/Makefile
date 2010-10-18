#	$OpenBSD: Makefile,v 1.11 2010/10/18 04:10:56 deraadt Exp $

.PATH:	${.CURDIR}/../bioctl

PROG=	ifconfig
SRCS=	ifconfig.c brconfig.c pbkdf2.c
MAN=	ifconfig.8

CPPFLAGS+=-DINET6 -I${.CURDIR}/../bioctl

.include <bsd.prog.mk>
