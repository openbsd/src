#	$OpenBSD: Makefile,v 1.2 2008/07/22 07:37:25 djm Exp $

.PATH: ${.CURDIR}/../bioctl
CFLAGS+=-Wall -I${.CURDIR}/../bioctl

PROG=	wpa-psk
MAN=	wpa-psk.8
SRCS=	wpa-psk.c pbkdf2.c

.include <bsd.prog.mk>
