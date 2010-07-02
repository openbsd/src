#	$OpenBSD: Makefile,v 1.3 2010/07/02 21:20:57 yasuoka Exp $

.include <bsd.own.mk>

PROG=		npppdctl
SRCS=		npppdctl.c debugutil.c
MAN=		npppdctl.8

CPPFLAGS+=	-I${.CURDIR}/../npppd -I${.CURDIR}/../common

.include <bsd.prog.mk>
.PATH:		${.CURDIR}/../common
